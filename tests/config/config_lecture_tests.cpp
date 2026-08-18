/*
 * config_lecture_tests.cpp
 * Lecture du fichier : défauts, clés, commentaires, brume, fichier Windows.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "config/aide_config.hpp"

#include "app/Config.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using artouste::app::Config;
using artouste::app::loadConfig;
using essais_config::clesDuModele;
using essais_config::effacerTemp;
using essais_config::lireTout;
using essais_config::preparerDossier;
using essais_config::writeTemp;

TEST_CASE("fichier absent : valeurs par défaut", "[config]") {
    const Config cfg = loadConfig("/chemin/qui/n/existe/pas/config.txt");
    REQUIRE(cfg.terrain == "ossau");
}

TEST_CASE("clé terrain lue", "[config]") {
    const auto path = writeTemp("artouste_cfg_terrain.txt", "terrain cote-landes\n");
    const Config cfg = loadConfig(path);
    REQUIRE(cfg.terrain == "cote-landes");
    effacerTemp(path);
}

TEST_CASE("commentaires et lignes vides ignorés", "[config]") {
    const auto path =
        writeTemp("artouste_cfg_comments.txt", "# un commentaire\n\n   \nterrain ossau\n# autre\n");
    const Config cfg = loadConfig(path);
    REQUIRE(cfg.terrain == "ossau");
    effacerTemp(path);
}

TEST_CASE("valeur vide : la clé garde son défaut", "[config]") {
    const auto path = writeTemp("artouste_cfg_empty.txt", "terrain   \n");
    const Config cfg = loadConfig(path);
    REQUIRE(cfg.terrain == "ossau");
    effacerTemp(path);
}

TEST_CASE("clé radio_url lue", "[config]") {
    const auto path =
        writeTemp("artouste_cfg_radio.txt", "radio_url https://exemple.test/flux.mp3\n");
    const Config cfg = loadConfig(path);
    REQUIRE(cfg.radioUrl == "https://exemple.test/flux.mp3");
    effacerTemp(path);
}

TEST_CASE("radio_url absente : chaîne vide par défaut", "[config]") {
    const Config cfg = loadConfig("/chemin/inexistant/config.txt");
    REQUIRE(cfg.radioUrl.empty());
}

TEST_CASE("brume : une fin avant le début est rétablie", "[config]") {
    /* Une brume qui finirait avant de commencer inverserait le fondu : le lointain
       redeviendrait net et le proche disparaîtrait. Le chargeur rétablit un ordre
       valide plutôt que d'ignorer les deux valeurs. */
    const auto path = writeTemp("artouste_cfg_brume.txt", "brume_debut 8000\nbrume_fin 2000\n");
    const Config cfg = loadConfig(path);
    CHECK(cfg.fogStartM == 8000.0f);
    CHECK(cfg.fogEndM > cfg.fogStartM);
    effacerTemp(path);
}

TEST_CASE("chaque option annoncée est réellement lue", "[config]") {
    /* Troisième garde-fou : app::clesConnues() pourrait annoncer une clé que le
       chargeur a oublié de traiter. On vérifie donc, clé par clé, qu'une valeur
       écrite dans le fichier arrive bien jusqu'à la structure Config. Ajouter une
       option au jeu, c'est ajouter une ligne ici. */
    struct Cas {
        std::string ligne;                      /* contenu du config.txt de test */
        std::function<bool(const Config&)> lue; /* la valeur est-elle arrivée ? */
    };
    const std::vector<std::pair<std::string, Cas>> cas = {
        {"terrain", {"terrain bordeaux", [](const Config& c) { return c.terrain == "bordeaux"; }}},
        {"turbine_demarree",
         {"turbine_demarree 1", [](const Config& c) { return c.turbineRunning; }}},
        {"demo", {"demo 1", [](const Config& c) { return c.demo; }}},
        {"arbres", {"arbres 0", [](const Config& c) { return !c.trees; }}},
        {"relief_fenetre",
         {"relief_fenetre 1", [](const Config& c) { return c.reliefWindow; }}},
        {"zone_hv", {"zone_hv 1", [](const Config& c) { return c.zoneHv; }}},
        {"verifier_maj", {"verifier_maj 0", [](const Config& c) { return !c.checkUpdate; }}},
        {"radio_url",
         {"radio_url https://exemple.test/f.mp3",
          [](const Config& c) { return c.radioUrl == "https://exemple.test/f.mp3"; }}},
        {"soleil_vitesse",
         {"soleil_vitesse 144", [](const Config& c) { return c.sunTimeScale == 144.0f; }}},
        {"lune_vitesse",
         {"lune_vitesse 3", [](const Config& c) { return c.nightSpeedFactor == 3.0f; }}},
        {"brume_debut",
         {"brume_debut 2000", [](const Config& c) { return c.fogStartM == 2000.0f; }}},
        {"brume_fin", {"brume_fin 9000", [](const Config& c) { return c.fogEndM == 9000.0f; }}},
        {"arbres_max",
         {"arbres_max 500000", [](const Config& c) { return c.treeBudget == 500000; }}},
        {"tuiles_fenetre_px",
         {"tuiles_fenetre_px 4096", [](const Config& c) { return c.detailWindowPx == 4096; }}},
        {"tuiles_dossier",
         {"tuiles_dossier /disque/tuiles",
          [](const Config& c) { return c.tilesDir == "/disque/tuiles"; }}},
        {"relief_sommets_max",
         {"relief_sommets_max 300000",
          [](const Config& c) { return c.reliefVertexBudget == 300000; }}},
        {"msaa", {"msaa 2", [](const Config& c) { return c.msaa == 2; }}},
    };

    /* Le tableau doit couvrir exactement les clés annoncées : sinon, une option
       ajoutée au code passerait au travers de cette vérification. */
    std::set<std::string> couvertes;
    for (const auto& [cle, essai] : cas) {
        couvertes.insert(cle);
    }
    REQUIRE(couvertes == artouste::app::clesConnues());

    for (const auto& [cle, essai] : cas) {
        const auto path = writeTemp("artouste_cfg_cle.txt", essai.ligne + "\n");
        const Config cfg = loadConfig(path);
        INFO("clé annoncée par app::clesConnues() mais non lue : " << cle);
        REQUIRE(essai.lue(cfg));
        effacerTemp(path);
    }
}

TEST_CASE("fichier enregistré depuis Windows : BOM UTF-8 et fins de ligne CRLF", "[config]") {
    /* Reproduit ce que produit le Bloc-notes Windows : BOM UTF-8 en tête et
       lignes terminées par \r\n. La première ligne (commentaire accentué) doit
       rester reconnue comme commentaire, et la clé doit être lue normalement. */
    const std::string content =
        "\xEF\xBB\xBF# commentaire accentué : vallée d'Ossau\r\nterrain cote-landes\r\n";
    const auto path = writeTemp("artouste_cfg_windows.txt", content);
    const Config cfg = loadConfig(path);
    REQUIRE(cfg.terrain == "cote-landes");
    effacerTemp(path);
}
