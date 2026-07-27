/*
 * texture_cache_tests.cpp
 * Compression BC7 (render/Bc7.hpp) et conteneur DDS (render/Dds.hpp). Ni l'un
 * ni l'autre n'a besoin d'un contexte graphique : la compression est du calcul
 * pur et le conteneur du fichier binaire, donc tout se teste hors du jeu.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "render/Bc7.hpp"
#include "render/Dds.hpp"

using namespace artouste::render;

namespace {

/* Une mire simple mais représentative d'une orthophoto : un dégradé continu,
   sur lequel on pose des carrés très contrastés. Le dégradé exerce
   l'interpolation des blocs, les carrés leurs transitions franches. */
std::vector<unsigned char> mire(int largeur, int hauteur) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(largeur) *
                                      static_cast<std::size_t>(hauteur) * 4);
    for (int y = 0; y < hauteur; ++y) {
        for (int x = 0; x < largeur; ++x) {
            /* Toute l'arithmétique en size_t : ne convertir que le premier terme
               laisse la largeur et l'abscisse en int, promus ensuite en silence. */
            const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(largeur) +
                                   static_cast<std::size_t>(x)) *
                                  4u;
            const bool        carre = ((x / 7) % 2) == ((y / 5) % 2);
            pixels[i + 0] = static_cast<unsigned char>(carre ? 250 : (x * 255) / largeur);
            pixels[i + 1] = static_cast<unsigned char>(carre ? 250 : (y * 255) / hauteur);
            pixels[i + 2] = static_cast<unsigned char>(carre ? 250 : 40);
            pixels[i + 3] = 255;
        }
    }
    return pixels;
}

/* Chemin temporaire propre à ce test, supprimé par le destructeur même si une
   assertion échoue en cours de route. */
class FichierTemporaire {
public:
    explicit FichierTemporaire(const char* nom)
        : m_chemin(std::filesystem::temp_directory_path() / nom) {
        std::error_code ec;
        std::filesystem::remove(m_chemin, ec);
    }
    ~FichierTemporaire() {
        std::error_code ec;
        std::filesystem::remove(m_chemin, ec);
    }
    FichierTemporaire(const FichierTemporaire&)            = delete;
    FichierTemporaire& operator=(const FichierTemporaire&) = delete;

    [[nodiscard]] const std::filesystem::path& chemin() const { return m_chemin; }

private:
    std::filesystem::path m_chemin;
};

}  /* namespace */

TEST_CASE("octetsBc7 compte un bloc de 16 octets par carré de 4x4 pixels") {
    CHECK(dds::octetsBc7(4, 4) == 16);
    CHECK(dds::octetsBc7(8, 8) == 64);
    /* Dimensions non multiples de 4 : le bloc de bordure est complet quand
       même, une image de 5 pixels de large occupe deux blocs. */
    CHECK(dds::octetsBc7(5, 4) == 32);
    CHECK(dds::octetsBc7(1, 1) == 16);
    CHECK(dds::octetsBc7(0, 8) == 0);
    CHECK(dds::octetsBc7(-4, 8) == 0);
}

TEST_CASE("La compression BC7 produit une chaîne de mipmaps complète") {
    constexpr int largeur = 64;
    constexpr int hauteur = 48;
    const auto    pixels  = mire(largeur, hauteur);

    const auto image = bc7::compresser(pixels.data(), largeur, hauteur, nullptr);
    REQUIRE(image.has_value());
    CHECK(image->largeur == largeur);
    CHECK(image->hauteur == hauteur);

    /* 64x48 se réduit en 32x24, 16x12, 8x6, 4x3, 2x1, 1x1 : sept niveaux. */
    REQUIRE(image->niveaux.size() == 7);
    CHECK(image->niveaux.front().largeur == 64);
    CHECK(image->niveaux.back().largeur == 1);
    CHECK(image->niveaux.back().hauteur == 1);

    /* Les niveaux se suivent sans trou ni recouvrement, et couvrent exactement
       le tampon. */
    std::size_t attendu = 0;
    for (const dds::Niveau& n : image->niveaux) {
        CHECK(n.decalage == attendu);
        CHECK(n.octets == dds::octetsBc7(n.largeur, n.hauteur));
        attendu += n.octets;
    }
    CHECK(image->donnees.size() == attendu);
}

TEST_CASE("La compression divise bien la mémoire par quatre") {
    constexpr int largeur = 128;
    constexpr int hauteur = 128;
    const auto    pixels  = mire(largeur, hauteur);

    const auto image = bc7::compresser(pixels.data(), largeur, hauteur, nullptr);
    REQUIRE(image.has_value());

    /* BC7 code un pixel sur un octet, contre quatre en RGBA : le niveau 0 doit
       peser le quart de la source. C'est tout l'intérêt du format. */
    CHECK(image->niveaux.front().octets == pixels.size() / 4);
}

TEST_CASE("La progression est croissante et atteint 1") {
    constexpr int largeur = 96;
    constexpr int hauteur = 96;
    const auto    pixels  = mire(largeur, hauteur);

    std::vector<float> vues;
    const auto         image = bc7::compresser(pixels.data(), largeur, hauteur,
                                               [&vues](float f) {
                                           vues.push_back(f);
                                           return true;
                                       });
    REQUIRE(image.has_value());
    REQUIRE(!vues.empty());

    for (std::size_t i = 1; i < vues.size(); ++i) {
        CHECK(vues[i] >= vues[i - 1]);
    }
    CHECK(vues.front() > 0.0f);
    CHECK(vues.back() == 1.0f);
}

TEST_CASE("Un rappel qui renvoie faux annule la compression") {
    constexpr int largeur = 96;
    constexpr int hauteur = 96;
    const auto    pixels  = mire(largeur, hauteur);

    int        appels = 0;
    const auto image  = bc7::compresser(pixels.data(), largeur, hauteur, [&appels](float) {
        ++appels;
        return false;  /* on annule dès la première bande */
    });
    CHECK(!image.has_value());
    CHECK(appels == 1);
}

TEST_CASE("La compression refuse des dimensions invalides") {
    const auto pixels = mire(8, 8);
    CHECK(!bc7::compresser(nullptr, 8, 8, nullptr).has_value());
    CHECK(!bc7::compresser(pixels.data(), 0, 8, nullptr).has_value());
    CHECK(!bc7::compresser(pixels.data(), 8, -1, nullptr).has_value());
}

TEST_CASE("Un DDS écrit puis relu rend exactement la même image") {
    FichierTemporaire fichier("artouste_test_cache.dds");
    const auto        pixels = mire(64, 48);
    const auto        image  = bc7::compresser(pixels.data(), 64, 48, nullptr);
    REQUIRE(image.has_value());

    const dds::Empreinte source{123456, 1700000000};
    REQUIRE(dds::ecrire(fichier.chemin(), *image, source));

    const auto relue = dds::lire(fichier.chemin(), source);
    REQUIRE(relue.has_value());
    CHECK(relue->largeur == image->largeur);
    CHECK(relue->hauteur == image->hauteur);
    REQUIRE(relue->niveaux.size() == image->niveaux.size());
    for (std::size_t i = 0; i < relue->niveaux.size(); ++i) {
        CHECK(relue->niveaux[i].largeur == image->niveaux[i].largeur);
        CHECK(relue->niveaux[i].hauteur == image->niveaux[i].hauteur);
        CHECK(relue->niveaux[i].decalage == image->niveaux[i].decalage);
        CHECK(relue->niveaux[i].octets == image->niveaux[i].octets);
    }
    /* On ne compare pas les deux vecteurs par ==. Catch2 imprimerait alors
       l'expansion complète, soit des milliers d'octets : illisible en cas
       d'échec, et il lève même un length_error en mode --success. On cherche
       donc l'indice du premier octet qui diffère, qui vaut la taille totale
       quand les tampons sont identiques et localise l'erreur sinon. */
    REQUIRE(relue->donnees.size() == image->donnees.size());
    const auto ecart = std::mismatch(relue->donnees.begin(), relue->donnees.end(),
                                     image->donnees.begin());
    const auto premiereDifference =
        static_cast<std::size_t>(std::distance(relue->donnees.begin(), ecart.first));
    CHECK(premiereDifference == relue->donnees.size());
}

TEST_CASE("Une empreinte différente périme le cache") {
    FichierTemporaire fichier("artouste_test_peremption.dds");
    const auto        pixels = mire(32, 32);
    const auto        image  = bc7::compresser(pixels.data(), 32, 32, nullptr);
    REQUIRE(image.has_value());
    REQUIRE(dds::ecrire(fichier.chemin(), *image, dds::Empreinte{1000, 42}));

    /* Une source retéléchargée change de taille, de date, ou des deux : chacun
       des trois cas doit invalider le cache. */
    CHECK(!dds::lire(fichier.chemin(), dds::Empreinte{1001, 42}).has_value());
    CHECK(!dds::lire(fichier.chemin(), dds::Empreinte{1000, 43}).has_value());
    CHECK(!dds::lire(fichier.chemin(), dds::Empreinte{2000, 99}).has_value());
    CHECK(dds::lire(fichier.chemin(), dds::Empreinte{1000, 42}).has_value());
}

TEST_CASE("Un fichier absent, étranger ou tronqué est refusé sans planter") {
    const auto absent = std::filesystem::temp_directory_path() / "artouste_test_absent.dds";
    CHECK(!dds::lire(absent, dds::Empreinte{1, 1}).has_value());

    FichierTemporaire etranger("artouste_test_etranger.dds");
    {
        std::FILE* f = std::fopen(etranger.chemin().string().c_str(), "wb");
        REQUIRE(f != nullptr);
        std::fputs("ceci n'est pas un DDS, meme de loin", f);
        std::fclose(f);
    }
    CHECK(!dds::lire(etranger.chemin(), dds::Empreinte{1, 1}).has_value());

    /* Un cache tronqué, comme en laisserait un lancement interrompu : les
       en-têtes sont valides mais les blocs manquent. La relecture doit le
       rejeter, sinon on enverrait au GPU un tampon incomplet. */
    FichierTemporaire tronque("artouste_test_tronque.dds");
    const auto        pixels = mire(32, 32);
    const auto        image  = bc7::compresser(pixels.data(), 32, 32, nullptr);
    REQUIRE(image.has_value());
    const dds::Empreinte source{7, 7};
    REQUIRE(dds::ecrire(tronque.chemin(), *image, source));
    std::filesystem::resize_file(tronque.chemin(), 144 + 32);  /* en-têtes + un bloc */
    CHECK(!dds::lire(tronque.chemin(), source).has_value());
}
