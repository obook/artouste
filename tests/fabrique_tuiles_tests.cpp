/*
 * fabrique_tuiles_tests.cpp
 * Choix de la finesse des tuiles de détail (app/cartes/FabriqueTuiles.hpp) : ce
 * qu'une carte a réellement à gagner à en recevoir. C'est le seul endroit qui
 * décide si le gestionnaire de cartes propose un téléchargement de plusieurs
 * centaines de mégaoctets, et à quelle finesse : du calcul et un fichier texte,
 * sans contexte graphique ni réseau.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

/* stb_image n'a d'implémentation que dans render/Texture.cpp, qui réclame un
   contexte graphique. Ce binaire n'en a pas : on la lui fournit ici, une fois. */
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <system_error>

#include "app/cartes/FabriqueTuiles.hpp"

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

/* Écrit le terrain.txt d'une carte carrée de côté donné, dont l'orthophoto
   d'ensemble fait orthoPx de haut. Seules ces deux valeurs comptent ici : le
   reste est le minimum qu'exige la lecture du calage. */
void ecrireCarte(const std::filesystem::path& dossier, float coteM, int orthoPx) {
    std::ofstream out(dossier / "terrain.txt", std::ios::trunc);
    out << "# carte d'essai\n";
    out << "width_m " << coteM << "\n";
    out << "height_m " << coteM << "\n";
    out << "lon_min -1.1\nlon_max -1.0\nlat_min 43.6\nlat_max 43.7\n";
    if (orthoPx > 0) {
        out << "ortho_width " << orthoPx << "\n";
        out << "ortho_height " << orthoPx << "\n";
    }
}

}  /* namespace */

TEST_CASE("une carte d'ensemble reçoit la finesse de compromis") {
    /* 18 km pour 5008 px : 3,6 m/px, comme les cartes de montagne livrées. Trois
       fois plus fin ferait 1,2 m/px, plus grossier que le compromis : c'est donc
       lui qui l'emporte. */
    const DossierTemporaire dossier("artouste_finesse_montagne");
    ecrireCarte(dossier.chemin(), 18000.0f, 5008);

    const Interet i = interet(dossier.chemin());
    CHECK(i.ortho > 3.5f);
    CHECK(i.ortho < 3.7f);
    CHECK(i.visee == FINESSE_LA_PLUS_GROSSIERE);
    CHECK(i.vaut);
}

TEST_CASE("une carte d'aérodrome reçoit elle aussi la borne") {
    /* 6 km pour 7200 px : 0,85 m/px. Son tiers, 0,28 m/px, reste plus grossier
       que la borne : c'est encore elle qui l'emporte. La carte a malgré tout de
       quoi gagner, et la fabrication doit rester proposée. */
    const DossierTemporaire dossier("artouste_finesse_aerodrome");
    ecrireCarte(dossier.chemin(), 6100.0f, 7200);

    const Interet i = interet(dossier.chemin());
    CHECK(i.visee == FINESSE_LA_PLUS_GROSSIERE);
    CHECK(i.vaut);
}

TEST_CASE("le tiers l'emporte quand l'orthophoto approche la borne") {
    /* 2 km pour 3000 px : 0,67 m/px. Son tiers, 0,22 m/px, tombe entre les deux
       bornes : c'est le seul cas où le rapport décide, et il vaut alors
       exactement le gain visé. */
    const DossierTemporaire dossier("artouste_finesse_tiers");
    ecrireCarte(dossier.chemin(), 2000.0f, 3000);

    const Interet i = interet(dossier.chemin());
    CHECK(i.visee < FINESSE_LA_PLUS_GROSSIERE);
    CHECK(i.visee > FINESSE_LA_PLUS_FINE);
    CHECK(i.ortho / i.visee == GAIN_VISE);
    CHECK(i.vaut);
}

TEST_CASE("une carte à la finesse de la source n'a rien à gagner") {
    /* 2 km pour 8000 px : 0,25 m/px, soit l'orthophoto de l'IGN elle-même. Même
       en visant 0,20 m/px, la borne fine, le gain resterait sous le minimum :
       la fabrication ne doit pas être proposée. */
    const DossierTemporaire dossier("artouste_finesse_arene");
    ecrireCarte(dossier.chemin(), 2000.0f, 8000);

    const Interet i = interet(dossier.chemin());
    CHECK(i.visee == FINESSE_LA_PLUS_FINE);
    CHECK_FALSE(i.vaut);
}

TEST_CASE("la finesse visée reste dans ses bornes") {
    /* Orthophoto très grossière (50 km pour 2000 px, soit 25 m/px) : le tiers
       en ferait 8,3 m/px, que la borne grossière ramène au compromis. */
    const DossierTemporaire dossier("artouste_finesse_immense");
    ecrireCarte(dossier.chemin(), 50000.0f, 2000);

    const Interet i = interet(dossier.chemin());
    CHECK(i.visee == FINESSE_LA_PLUS_GROSSIERE);
    CHECK(i.vaut);
}

TEST_CASE("une carte qu'on ne sait pas mesurer garde le bénéfice du doute") {
    /* terrain.txt sans ortho_height : on ne peut pas comparer, on s'en tient au
       compromis plutôt que de refuser une carte sur une mesure absente. */
    const DossierTemporaire dossier("artouste_finesse_muette");
    ecrireCarte(dossier.chemin(), 18000.0f, 0);

    const Interet i = interet(dossier.chemin());
    CHECK(i.ortho == 0.0f);
    CHECK(i.visee == FINESSE_LA_PLUS_GROSSIERE);
    CHECK(i.vaut);
}

TEST_CASE("un dossier sans terrain.txt ne fait pas échouer la mesure") {
    const DossierTemporaire dossier("artouste_finesse_vide");

    const Interet i = interet(dossier.chemin());
    CHECK(i.visee == FINESSE_LA_PLUS_GROSSIERE);
    CHECK(i.vaut);
}

TEST_CASE("l'estimation compte les tuiles qui couvrent l'emprise") {
    /* 6100 m à 0,28 m/px : une tuile de 512 px couvre 143,4 m, il en faut donc
       43 par côté, la dernière rangée dépassant le bord. */
    const DossierTemporaire dossier("artouste_estimation");
    ecrireCarte(dossier.chemin(), 6100.0f, 7200);

    const Estimation e = estimer(dossier.chemin(), 0.28f);
    REQUIRE(e.valide);
    CHECK(e.colonnes == 43);
    CHECK(e.rangees == 43);
    CHECK(e.octetsDisque > 600ull * 1000ull * 1000ull);
    CHECK_FALSE(e.detail.empty());
}

TEST_CASE("une finesse absurde ne produit aucune estimation") {
    const DossierTemporaire dossier("artouste_estimation_nulle");
    ecrireCarte(dossier.chemin(), 6100.0f, 7200);

    CHECK_FALSE(estimer(dossier.chemin(), 0.0f).valide);
    CHECK_FALSE(estimer(dossier.chemin(), -1.0f).valide);
}

TEST_CASE("le résumé rend le poids du jeu sans le repeser") {
    const DossierTemporaire dossier("artouste_resume");
    std::filesystem::create_directories(dossier.chemin() / "12");
    {
        std::ofstream a(dossier.chemin() / "index.txt");
        a << "0123456789";
    }
    {
        std::ofstream b(dossier.chemin() / "12" / "34.dds", std::ios::binary);
        b << "01234";
    }

    /* Absent : rien à annoncer, l'appelant retombe sur la marche de dossier. */
    CHECK(lireResume(dossier.chemin()) == 0);

    ecrireResume(dossier.chemin());
    CHECK(lireResume(dossier.chemin()) == 15);
}

TEST_CASE("un résumé illisible vaut un résumé absent") {
    const DossierTemporaire dossier("artouste_resume_casse");
    std::ofstream(dossier.chemin() / NOM_RESUME) << "# rien que des commentaires\n";

    CHECK(lireResume(dossier.chemin()) == 0);
}
