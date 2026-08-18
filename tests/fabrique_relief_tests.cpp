/*
 * fabrique_relief_tests.cpp
 * Choix du pas des tuiles de relief et format des tuiles écrites
 * (app/cartes/FabriqueRelief.hpp). Deux points qui se cassent en silence :
 * un pas qui ne s'emboîte pas dans la maille de la carte ne se voit qu'en vol,
 * à la frontière de la fenêtre, et une tuile mal encodée ne se voit nulle part.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

#include "app/cartes/FabriqueRelief.hpp"
#include "app/cartes/fabrique/FabriqueReliefInterne.hpp"
#include "render/relief/CalageRelief.hpp"
#include "render/relief/FenetreRelief.hpp"
#include "render/relief/FenetreReliefInterne.hpp"

using namespace artouste::app::cartes;

namespace {

/* Dossier temporaire propre à un test, effacé même si une assertion échoue. */
class DossierTemporaire {
public:
    explicit DossierTemporaire(const char* nom)
        : m_chemin(std::filesystem::temp_directory_path() / nom) {
        std::error_code ec;
        std::filesystem::remove_all(m_chemin, ec);
        std::filesystem::create_directories(m_chemin, ec);
    }
    ~DossierTemporaire() {
        std::error_code ec;
        std::filesystem::remove_all(m_chemin, ec);
    }
    DossierTemporaire(const DossierTemporaire&)            = delete;
    DossierTemporaire& operator=(const DossierTemporaire&) = delete;

    [[nodiscard]] const std::filesystem::path& chemin() const { return m_chemin; }

private:
    std::filesystem::path m_chemin;
};

/* Le terrain.txt d'une carte : seuls l'emprise et le maillage comptent ici. */
void ecrireCarte(const std::filesystem::path& dossier, float largeurM, float hauteurM,
                 int colonnes, int rangees) {
    std::ofstream out(dossier / "terrain.txt", std::ios::trunc);
    out << "# carte d'essai\n";
    out << "cols " << colonnes << "\nrows " << rangees << "\n";
    out << "width_m " << largeurM << "\nheight_m " << hauteurM << "\n";
    out << "elev_min 0.00\nelev_max 2868.62\n";
    out << "lon_min 0.05\nlon_max 0.27\nlat_min 42.85\nlat_max 43.02\n";
}

} /* namespace */

TEST_CASE("Le pas du relief s'emboîte dans la maille de la carte") {
    DossierTemporaire dossier("artouste_relief_pas");
    /* Le Pic du Midi de Bigorre : 2048 points sur 17,9 x 18,9 km. */
    ecrireCarte(dossier.chemin(), 17930.1f, 18924.4f, 2048, 2048);

    const GrilleRelief g = grilleRelief(dossier.chemin());
    REQUIRE(g.valide);

    const float mailleX = 17930.1f / 2047.0f;
    const float mailleZ = 18924.4f / 2047.0f;
    CHECK(artouste::render::relief::emboiteDansMaille(g.pasX, mailleX,
                                                      artouste::render::relief::PAS_ANNEAU));
    CHECK(artouste::render::relief::emboiteDansMaille(g.pasZ, mailleZ,
                                                      artouste::render::relief::PAS_ANNEAU));
    /* Et au plus près de 2 m : c'est le pas que les scripts ont retenu. */
    CHECK(std::fabs(g.pasX - 2.1898f) < 1e-3f);
    CHECK(std::fabs(g.pasZ - 2.3112f) < 1e-3f);
    /* 512 points de 2,19 m couvrent 1121 m : seize tuiles suffisent, la maille
       de la carte et la tuile tombant presque juste (2047 contre 2048). */
    CHECK(g.colonnes == 16);
    CHECK(g.rangees == 16);
}

TEST_CASE("Le pas reste emboîté sur une carte à maille fine") {
    DossierTemporaire dossier("artouste_relief_pas_fin");
    /* Banc d'essai du Pic du Midi : 3 km sur 1536 points, soit 1,95 m de maille.
       Le pas ne peut alors que la diviser par 4 au minimum. */
    ecrireCarte(dossier.chemin(), 2999.1f, 3005.6f, 1536, 1536);

    const GrilleRelief g = grilleRelief(dossier.chemin());
    REQUIRE(g.valide);
    CHECK(artouste::render::relief::emboiteDansMaille(g.pasX, 2999.1f / 1535.0f,
                                                      artouste::render::relief::PAS_ANNEAU));
    CHECK(g.pasX < 2.0f);
}

TEST_CASE("Une carte sans maillage ne donne pas de grille") {
    DossierTemporaire dossier("artouste_relief_sans_maillage");
    std::ofstream     out(dossier.chemin() / "terrain.txt", std::ios::trunc);
    out << "width_m 1000\nheight_m 1000\n";
    out << "lon_min 0\nlon_max 0.01\nlat_min 43\nlat_max 43.01\n";
    out.close();

    CHECK_FALSE(grilleRelief(dossier.chemin()).valide);
    CHECK_FALSE(estimerRelief(dossier.chemin()).valide);
}

TEST_CASE("L'index écrit est celui que l'on relit") {
    DossierTemporaire dossier("artouste_relief_index");
    ecrireCarte(dossier.chemin(), 17930.1f, 18924.4f, 2048, 2048);
    const GrilleRelief voulue = grilleRelief(dossier.chemin());
    REQUIRE(voulue.valide);

    CalageCarte carte;
    carte.largeurM = 17930.1f;
    carte.hauteurM = 18924.4f;
    REQUIRE(ecrireIndexRelief(dossier.chemin(), "essai", carte, voulue));

    const GrilleRelief relue = lireIndexRelief(dossier.chemin());
    REQUIRE(relue.valide);
    CHECK(relue.colonnes == voulue.colonnes);
    CHECK(relue.rangees == voulue.rangees);
    CHECK(std::fabs(relue.pasX - voulue.pasX) < 1e-4f);
    CHECK(std::fabs(relue.pasZ - voulue.pasZ) < 1e-4f);
}

TEST_CASE("Une tuile de relief se relit au centimètre") {
    DossierTemporaire dossier("artouste_relief_tuile");
    const int         cote   = RELIEF_TUILE_POINTS;
    const std::size_t points = static_cast<std::size_t>(cote) * static_cast<std::size_t>(cote);

    /* Une tuile de montagne, 1300 m de dénivelé : l'argument même du choix de
       16 bits par point est que la quantification reste sous 3 cm. */
    std::vector<float>         altitudes(points);
    std::vector<unsigned char> manquant(points, 0);
    for (int j = 0; j < cote; ++j) {
        for (int i = 0; i < cote; ++i) {
            altitudes[static_cast<std::size_t>(j) * static_cast<std::size_t>(cote) +
                      static_cast<std::size_t>(i)] =
                1000.0f + 1300.0f * (static_cast<float>(i) / 511.0f) *
                              (static_cast<float>(j) / 511.0f);
        }
    }

    std::uintmax_t octets = 0;
    CHECK(ecrireBlocRelief(dossier.chemin(), altitudes, manquant, 3, 5, 1, 1, 2.1898f, 2.3112f,
                           octets) == 1);
    CHECK(octets > 0);

    std::ifstream in(dossier.chemin() / "5" / "3.r16", std::ios::binary);
    REQUIRE(in);
    std::vector<unsigned char> brut(artouste::render::relief::EN_TETE_V2_OCTETS + points * 2);
    in.read(reinterpret_cast<char*>(brut.data()), static_cast<std::streamsize>(brut.size()));
    REQUIRE(static_cast<std::size_t>(in.gcount()) == brut.size());

    CHECK(std::memcmp(brut.data(), artouste::render::relief::MAGIQUE, 4) == 0);
    CHECK(artouste::render::relief::lire16(brut.data() + 4) == 2);
    CHECK(artouste::render::relief::lire16(brut.data() + 6) == cote);
    CHECK(std::fabs(artouste::render::relief::lireFlottant(brut.data() + 8) - 2.1898f) < 1e-4f);
    CHECK(std::fabs(artouste::render::relief::lireFlottant(brut.data() + 12) - 2.3112f) < 1e-4f);

    const float mini    = artouste::render::relief::lireFlottant(brut.data() + 16);
    const float etendue = artouste::render::relief::lireFlottant(brut.data() + 20);
    const unsigned char* niveaux = brut.data() + artouste::render::relief::EN_TETE_V2_OCTETS;
    float                ecartMax = 0.0f;
    for (std::size_t k = 0; k < points; ++k) {
        const float relue =
            mini + static_cast<float>(artouste::render::relief::lire16(niveaux + 2 * k)) /
                       65535.0f * etendue;
        ecartMax = std::max(ecartMax, std::fabs(relue - altitudes[k]));
    }
    CHECK(ecartMax < 0.03f);
}

TEST_CASE("Une tuile entièrement hors couverture n'est pas écrite") {
    DossierTemporaire dossier("artouste_relief_trou");
    const std::size_t points = static_cast<std::size_t>(RELIEF_TUILE_POINTS) *
                               static_cast<std::size_t>(RELIEF_TUILE_POINTS);
    const std::vector<float>         altitudes(points, 1200.0f);
    const std::vector<unsigned char> manquant(points, 1);

    std::uintmax_t octets = 0;
    CHECK(ecrireBlocRelief(dossier.chemin(), altitudes, manquant, 0, 0, 1, 1, 2.0f, 2.0f,
                           octets) == 0);
    CHECK(octets == 0);
    CHECK_FALSE(std::filesystem::exists(dossier.chemin() / "0" / "0.r16"));
}
