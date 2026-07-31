/*
 * ligne_commande_tests.cpp
 * Lecture des options de lancement et normalisation des noms de monuments.
 * Le second point compte autant que le premier : c'est lui qui décide si
 * "pantheon" tapé au shell retrouve "Panthéon" dans monuments.txt.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include <catch2/catch_test_macros.hpp>

#include "app/LigneCommande.hpp"

#include <vector>

using artouste::app::lireLigneCommande;
using artouste::app::normaliserNom;
using artouste::app::OptionsLancement;

namespace {

/* Appelle le lecteur d'options comme le ferait main(), argv[0] compris. */
OptionsLancement lire(std::vector<const char*> args) {
    args.insert(args.begin(), "artouste");
    return lireLigneCommande(static_cast<int>(args.size()),
                             const_cast<char**>(args.data()));
}

} /* namespace */

TEST_CASE("Sans argument, aucune option n'est demandée", "[ligne_commande]") {
    const OptionsLancement o = lire({});
    CHECK_FALSE(o.erreur);
    CHECK_FALSE(o.aide);
    CHECK(o.carte.empty());
    CHECK_FALSE(o.aPointDapparition());
    /* Le menu de démarrage doit s'ouvrir : c'est le lancement normal. */
    CHECK_FALSE(o.sauteMenu());
}

TEST_CASE("La carte et le point d'apparition sont lus", "[ligne_commande]") {
    const OptionsLancement o = lire({"--carte", "paris", "--monument", "Pantheon",
                                     "--alt", "250", "--cap", "270"});
    REQUIRE_FALSE(o.erreur);
    CHECK(o.carte == "paris");
    CHECK(o.monument == "Pantheon");
    CHECK(o.aAltitude);
    CHECK(o.altitude == 250.0f);
    CHECK(o.aCap);
    CHECK(o.cap == 270.0f);
    CHECK(o.aPointDapparition());
    CHECK(o.sauteMenu());
}

TEST_CASE("Les formes courtes valent les longues", "[ligne_commande]") {
    const OptionsLancement o = lire({"-c", "bigorre", "-l", "Pic du Midi", "-a", "0"});
    REQUIRE_FALSE(o.erreur);
    CHECK(o.carte == "bigorre");
    CHECK(o.lieu == "Pic du Midi");
    /* --alt 0 pose au sol : il faut le distinguer de l'absence d'option, sans
       quoi on ne pourrait jamais demander une apparition posée. */
    CHECK(o.aAltitude);
    CHECK(o.altitude == 0.0f);
}

TEST_CASE("--carte seule suffit à sauter le menu", "[ligne_commande]") {
    const OptionsLancement o = lire({"--carte", "arcachon"});
    REQUIRE_FALSE(o.erreur);
    CHECK(o.sauteMenu());
    /* Mais sans point d'apparition : le vol commence au pad, turbine coupée. */
    CHECK_FALSE(o.aPointDapparition());
}

TEST_CASE("Les coordonnées explicites vont par paire", "[ligne_commande]") {
    const OptionsLancement bonnes =
        lire({"--lon", "2.346073", "--lat", "48.846167"});
    REQUIRE_FALSE(bonnes.erreur);
    CHECK(bonnes.aLonLat);
    CHECK(bonnes.aPointDapparition());

    /* Une longitude seule placerait l'appareil sur l'équateur, à des milliers de
       kilomètres de la carte : c'est une erreur, pas un défaut à combler. */
    CHECK(lire({"--lon", "2.346073"}).erreur);
    CHECK(lire({"--lat", "48.846167"}).erreur);
}

TEST_CASE("Une option inconnue ou sans valeur est une erreur", "[ligne_commande]") {
    CHECK(lire({"--nawak"}).erreur);
    CHECK(lire({"--carte"}).erreur);       /* valeur manquante en fin de ligne */
    CHECK(lire({"--alt"}).erreur);
}

TEST_CASE("L'aide s'arrête là", "[ligne_commande]") {
    const OptionsLancement o = lire({"--aide", "--carte", "paris"});
    CHECK(o.aide);
    CHECK_FALSE(o.erreur);
    /* Rien n'est lu après --aide : on affiche et on sort. */
    CHECK(o.carte.empty());
}

TEST_CASE("La normalisation ignore casse, accents et ponctuation",
          "[ligne_commande]") {
    CHECK(normaliserNom("Panthéon") == "pantheon");
    CHECK(normaliserNom("PANTHEON") == "pantheon");
    CHECK(normaliserNom("Sacré-Coeur") == "sacrecoeur");
    CHECK(normaliserNom("Sacre Coeur") == "sacrecoeur");
    CHECK(normaliserNom("Église de la Madeleine") == "eglisedelamadeleine");
    CHECK(normaliserNom("Notre-Dame de Paris") == "notredamedeparis");
    CHECK(normaliserNom("Hôtel des Invalides") == "hoteldesinvalides");
    CHECK(normaliserNom("Opéra Garnier") == "operagarnier");
}

TEST_CASE("La ligature oe se normalise comme les deux lettres",
          "[ligne_commande]") {
    /* Le nom peut s'écrire avec la ligature dans monuments.txt et sans elle au
       shell, ou l'inverse : les deux doivent se rejoindre. */
    CHECK(normaliserNom("Sacré-Cœur") == normaliserNom("Sacré-Coeur"));
    CHECK(normaliserNom("cœur") == "coeur");
}

TEST_CASE("Un fragment normalisé retrouve le nom complet", "[ligne_commande]") {
    /* C'est la propriété dont dépend la recherche : resoudrePointDapparition
       fait un find() du fragment cherché dans le nom normalisé du monument. */
    CHECK(normaliserNom("Grande Arche de la Défense")
              .find(normaliserNom("arche")) != std::string::npos);
    CHECK(normaliserNom("Tour Eiffel").find(normaliserNom("eiffel")) !=
          std::string::npos);
    CHECK(normaliserNom("Panthéon").find(normaliserNom("panth")) !=
          std::string::npos);
}

TEST_CASE("Un nom vide ne cherche rien", "[ligne_commande]") {
    /* Sinon le fragment vide serait trouvé dans le premier monument venu, et
       --monument "" ferait apparaître l'appareil n'importe où. */
    CHECK(normaliserNom("").empty());
    CHECK(normaliserNom("---").empty());
}
