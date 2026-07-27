/*
 * ortho_tiles_tests.cpp
 * Jeu de tuiles de détail d'une carte (render/tuiles/Pyramide.hpp) : index sur
 * disque et passage des coordonnées monde aux indices de tuile. Aucun contexte
 * graphique nécessaire, c'est du calcul et du fichier texte.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <system_error>

#include "render/tuiles/Pyramide.hpp"

using namespace artouste::render::tuiles;

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

/* Calage d'essai : tuiles de 512 px à 0,5 m/px, soit 256 m au sol, sur une
   grille de 4 x 3 tuiles dont le coin nord-ouest est à (-512, -384). */
Calage calageEssai() {
    Calage c;
    c.tuilePx   = 512;
    c.mParPixel = 0.5f;
    c.colonnes  = 4;
    c.rangees   = 3;
    c.coinX     = -512.0f;
    c.coinZ     = -384.0f;
    return c;
}

}  /* namespace */

TEST_CASE("un calage sans finesse ni grille est refusé") {
    CHECK_FALSE(Calage{}.valide());

    Calage c = calageEssai();
    CHECK(c.valide());

    c.mParPixel = 0.0f;
    CHECK_FALSE(c.valide());

    c           = calageEssai();
    c.colonnes  = 0;
    CHECK_FALSE(c.valide());

    /* Un côté de tuile qui n'est pas multiple de 4 ne se découpe pas en blocs
       BC7 entiers : la mise à jour partielle de la fenêtre serait impossible. */
    c         = calageEssai();
    c.tuilePx = 510;
    CHECK_FALSE(c.valide());
}

TEST_CASE("une tuile couvre son côté au sol, pas un pixel de plus") {
    const Pyramide p{"/tmp/inexistant", calageEssai()};
    CHECK(p.calage().tuileM() == 256.0f);
    CHECK(p.largeurM() == 1024.0f);
    CHECK(p.hauteurM() == 768.0f);
}

TEST_CASE("chaque point de l'emprise tombe dans sa tuile") {
    const Pyramide p{"/tmp/inexistant", calageEssai()};
    int            col = -1, rangee = -1;

    /* Coin nord-ouest exact : première tuile. */
    REQUIRE(p.tuileEn(-512.0f, -384.0f, col, rangee));
    CHECK(col == 0);
    CHECK(rangee == 0);

    /* Un mètre avant la frontière est de la tuile 0 : toujours la tuile 0. */
    REQUIRE(p.tuileEn(-512.0f + 255.0f, -384.0f, col, rangee));
    CHECK(col == 0);

    /* La frontière elle-même appartient à la tuile suivante. */
    REQUIRE(p.tuileEn(-512.0f + 256.0f, -384.0f, col, rangee));
    CHECK(col == 1);

    /* Dernière tuile de la grille (colonne 3, rangée 2). */
    REQUIRE(p.tuileEn(511.0f, 383.0f, col, rangee));
    CHECK(col == 3);
    CHECK(rangee == 2);
}

TEST_CASE("un point hors emprise n'a pas de tuile") {
    const Pyramide p{"/tmp/inexistant", calageEssai()};
    int            col = 7, rangee = 7;

    /* Juste à l'ouest et juste au nord du coin d'ancrage : la division doit
       aller au plancher, sans quoi un indice négatif serait tronqué à 0 et le
       point serait cru dans la première tuile. */
    CHECK_FALSE(p.tuileEn(-513.0f, 0.0f, col, rangee));
    CHECK_FALSE(p.tuileEn(0.0f, -385.0f, col, rangee));
    /* Au-delà du bord est et du bord sud. */
    CHECK_FALSE(p.tuileEn(512.0f, 0.0f, col, rangee));
    CHECK_FALSE(p.tuileEn(0.0f, 384.0f, col, rangee));

    /* Les indices ne sont pas touchés en cas d'échec. */
    CHECK(col == 7);
    CHECK(rangee == 7);
}

TEST_CASE("le coin d'une tuile se déduit de ses indices") {
    const Pyramide p{"/tmp/inexistant", calageEssai()};
    float          x = 0.0f, z = 0.0f;

    p.coinTuile(0, 0, x, z);
    CHECK(x == -512.0f);
    CHECK(z == -384.0f);

    p.coinTuile(2, 1, x, z);
    CHECK(x == 0.0f);
    CHECK(z == -128.0f);
}

TEST_CASE("dansGrille borne les deux indices") {
    const Pyramide p{"/tmp/inexistant", calageEssai()};
    CHECK(p.dansGrille(0, 0));
    CHECK(p.dansGrille(3, 2));
    CHECK_FALSE(p.dansGrille(4, 2));
    CHECK_FALSE(p.dansGrille(3, 3));
    CHECK_FALSE(p.dansGrille(-1, 0));
    CHECK_FALSE(p.dansGrille(0, -1));
}

TEST_CASE("une tuile a un fichier par rangée-dossier") {
    const Pyramide p{"/tuiles/capbreton", calageEssai()};
    CHECK(p.fichier(0, 0) == std::filesystem::path("/tuiles/capbreton/0/0.dds"));
    CHECK(p.fichier(3, 2) == std::filesystem::path("/tuiles/capbreton/2/3.dds"));
}

TEST_CASE("l'index écrit se relit à l'identique") {
    const DossierTemporaire dossier("artouste_tuiles_index");
    const Pyramide          ecrite{dossier.chemin(), calageEssai()};
    REQUIRE(ecrite.ecrireIndex());

    const auto relue = Pyramide::ouvrir(dossier.chemin());
    REQUIRE(relue.has_value());
    CHECK(relue->calage().tuilePx == 512);
    CHECK(relue->calage().mParPixel == 0.5f);
    CHECK(relue->calage().colonnes == 4);
    CHECK(relue->calage().rangees == 3);
    CHECK(relue->calage().coinX == -512.0f);
    CHECK(relue->calage().coinZ == -384.0f);
    CHECK(relue->dossier() == dossier.chemin());
}

TEST_CASE("un index absent, vide ou incohérent n'ouvre rien") {
    const DossierTemporaire dossier("artouste_tuiles_index_casse");

    /* Absent. */
    CHECK_FALSE(Pyramide::ouvrir(dossier.chemin()).has_value());

    /* Présent mais sans aucune clé exploitable. */
    {
        std::ofstream out(dossier.chemin() / NOM_INDEX, std::ios::trunc);
        out << "# rien que des commentaires\n";
    }
    CHECK_FALSE(Pyramide::ouvrir(dossier.chemin()).has_value());

    /* Grille décrite mais finesse manquante : inutilisable. */
    {
        std::ofstream out(dossier.chemin() / NOM_INDEX, std::ios::trunc);
        out << "tuile_px 512\ncolonnes 4\nrangees 3\n";
    }
    CHECK_FALSE(Pyramide::ouvrir(dossier.chemin()).has_value());
}

TEST_CASE("une clé inconnue est ignorée sans casser la lecture") {
    const DossierTemporaire dossier("artouste_tuiles_index_futur");
    {
        std::ofstream out(dossier.chemin() / NOM_INDEX, std::ios::trunc);
        out << "tuile_px 512\n";
        out << "m_par_pixel 0.25\n";
        out << "niveaux 3 1 2 4\n";  /* clé d'une version future, avec sa valeur */
        out << "colonnes 8\n";
        out << "rangees 6\n";
        out << "coin_x 100\n";
        out << "coin_z 200\n";
    }

    const auto relue = Pyramide::ouvrir(dossier.chemin());
    REQUIRE(relue.has_value());
    CHECK(relue->calage().mParPixel == 0.25f);
    CHECK(relue->calage().colonnes == 8);
    CHECK(relue->calage().coinX == 100.0f);
}
