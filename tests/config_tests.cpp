/*
 * config_tests.cpp
 * Vérifie le chargeur de configuration "clé valeur" (app::loadConfig) : valeur
 * par défaut quand le fichier est absent, lecture de la clé terrain, et bonne
 * gestion des commentaires et lignes vides.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Config.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using artouste::app::Config;
using artouste::app::loadConfig;

namespace {

/* Écrit un fichier de configuration temporaire et renvoie son chemin. Chaque
   appel a son propre dossier : le chargeur écrit à côté du fichier qu'on lui
   donne (modèle réécrit, options ajoutées), et deux essais ne doivent pas se
   marcher dessus -- ni salir le dossier temporaire du système. */
std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const std::filesystem::path dossier = std::filesystem::temp_directory_path() / (name + ".d");
    std::filesystem::remove_all(dossier);
    std::filesystem::create_directories(dossier);
    const std::filesystem::path path = dossier / "config.txt";
    /* Mode binaire : sans lui, Windows traduit chaque \n en \r\n à l'écriture, et
       le fichier ne contient plus les octets que le test croit y avoir mis. Les
       essais sur les fins de ligne, eux, doivent poser exactement ce qu'ils
       annoncent. */
    std::ofstream out(path, std::ios::binary);
    out << content;
    return path;
}

/* Efface le dossier d'un fichier rendu par writeTemp. */
void effacerTemp(const std::filesystem::path& path) {
    std::filesystem::remove_all(path.parent_path());
}

} /* namespace */

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

namespace {

/* Lit un fichier en entier, pour vérifier ce que le chargeur y a écrit. */
std::string lireTout(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/* Prépare un dossier de test contenant un modèle config.default.txt et une
   configuration personnelle incomplète, comme celle d'un utilisateur ayant
   installé une version plus ancienne. */
std::filesystem::path preparerDossier(const std::string& nom,
                                      const std::string& configPersonnelle) {
    const std::filesystem::path dossier = std::filesystem::temp_directory_path() / nom;
    std::filesystem::remove_all(dossier);
    std::filesystem::create_directories(dossier);
    std::ofstream modele(dossier / "config.default.txt", std::ios::binary);
    modele << "# terrain : carte chargée au lancement.\n"
              "terrain ossau\n"
              "\n"
              "# verifier_maj : recherche d'une mise à jour au lancement.\n"
              "# 0 pour ne rien demander au réseau.\n"
              "verifier_maj 1\n";
    modele.close();
    std::ofstream perso(dossier / "config.txt", std::ios::binary);
    perso << configPersonnelle;
    return dossier;
}

} /* namespace */

TEST_CASE("option nouvelle ajoutée à une configuration ancienne", "[config]") {
    const auto dossier = preparerDossier("artouste_cfg_maj", "terrain bordeaux\n");
    const Config cfg = loadConfig(dossier / "config.txt");

    /* Le réglage existant est respecté, et l'option absente garde son défaut. */
    REQUIRE(cfg.terrain == "bordeaux");
    REQUIRE(cfg.checkUpdate);

    const std::string apres = lireTout(dossier / "config.txt");
    /* L'option manquante a été recopiée du modèle, avec sa documentation. */
    REQUIRE(apres.find("verifier_maj 1") != std::string::npos);
    REQUIRE(apres.find("# verifier_maj : recherche d'une mise à jour") != std::string::npos);
    REQUIRE(apres.find("# 0 pour ne rien demander au réseau.") != std::string::npos);
    /* Le choix de l'utilisateur n'a pas été réécrit, et la clé qu'il avait déjà
       n'a pas été ajoutée une seconde fois. */
    REQUIRE(apres.find("terrain bordeaux") != std::string::npos);
    REQUIRE(apres.find("terrain ossau") == std::string::npos);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("configuration complète : le fichier n'est pas touché", "[config]") {
    const auto dossier =
        preparerDossier("artouste_cfg_complet", "terrain bordeaux\nverifier_maj 0\n");
    const std::string avant = lireTout(dossier / "config.txt");

    const Config cfg = loadConfig(dossier / "config.txt");
    REQUIRE_FALSE(cfg.checkUpdate);
    REQUIRE(lireTout(dossier / "config.txt") == avant);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("deux lancements de suite : pas de doublon", "[config]") {
    const auto dossier = preparerDossier("artouste_cfg_deuxfois", "terrain bordeaux\n");
    loadConfig(dossier / "config.txt");
    const std::string apresPremier = lireTout(dossier / "config.txt");
    loadConfig(dossier / "config.txt");
    REQUIRE(lireTout(dossier / "config.txt") == apresPremier);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("configuration en CRLF : les lignes ajoutées le sont aussi", "[config]") {
    /* Fichier enregistré par le Bloc-notes de Windows. Mêler des fins de ligne LF
       aux siennes donnerait un fichier illisible dans certains éditeurs. */
    const auto dossier = preparerDossier("artouste_cfg_crlf", "terrain bordeaux\r\n");
    loadConfig(dossier / "config.txt");

    const std::string apres = lireTout(dossier / "config.txt");
    REQUIRE(apres.find("verifier_maj 1\r\n") != std::string::npos);
    /* Aucun saut de ligne isolé : chaque \n doit être précédé d'un \r. */
    for (std::size_t i = 0; i < apres.size(); ++i) {
        if (apres[i] == '\n') {
            REQUIRE(i > 0);
            REQUIRE(apres[i - 1] == '\r');
        }
    }

    std::filesystem::remove_all(dossier);
}

TEST_CASE("configuration sans saut de ligne final : l'ajout ne colle pas à la dernière clé",
          "[config]") {
    const auto dossier = preparerDossier("artouste_cfg_sansfin", "terrain bordeaux");
    const Config cfg = loadConfig(dossier / "config.txt");
    REQUIRE(cfg.terrain == "bordeaux");

    const std::string apres = lireTout(dossier / "config.txt");
    REQUIRE(apres.find("terrain bordeaux\n") != std::string::npos);

    /* Relu, le fichier complété doit rendre exactement les mêmes réglages. */
    const Config relu = loadConfig(dossier / "config.txt");
    REQUIRE(relu.terrain == "bordeaux");
    REQUIRE(relu.checkUpdate);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("modèle abîmé : sa saleté n'entre pas dans la configuration", "[config]") {
    /* Modèle écrasé par autre chose : clés inventées, valeur absurde, octets
       binaires. Rien de tout cela ne doit atterrir dans le fichier de
       l'utilisateur ; les options nouvelles lui viennent alors de la copie que le
       simulateur porte en lui. */
    const auto dossier = preparerDossier("artouste_cfg_abime", "terrain bordeaux\n");
    {
        std::ofstream modele(dossier / "config.default.txt", std::ios::binary);
        modele << "# notes de quelqu'un d'autre\nblabla 42\nmsaa 999\n\x01\x02binaire\n";
    }

    const Config cfg = loadConfig(dossier / "config.txt");
    REQUIRE(cfg.terrain == "bordeaux");

    const std::string apres = lireTout(dossier / "config.txt");
    REQUIRE(apres.find("blabla") == std::string::npos);
    REQUIRE(apres.find("msaa 999") == std::string::npos);
    REQUIRE(apres.find("binaire") == std::string::npos);
    REQUIRE(apres.find('\x01') == std::string::npos);
    /* Le réglage de l'utilisateur est intact, et les vraies options sont là. */
    REQUIRE(apres.find("terrain bordeaux") != std::string::npos);
    REQUIRE(apres.find("verifier_maj 1") != std::string::npos);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("modèle effacé : il est réécrit depuis la copie du simulateur", "[config]") {
    const auto dossier = preparerDossier("artouste_cfg_sansmodele", "terrain bordeaux\n");
    std::filesystem::remove(dossier / "config.default.txt");

    const Config cfg = loadConfig(dossier / "config.txt");
    REQUIRE(cfg.terrain == "bordeaux");

    /* Le modèle est de retour sur le disque, avec toutes les options du jeu... */
    REQUIRE(std::filesystem::exists(dossier / "config.default.txt"));
    const std::string modeleRelu = lireTout(dossier / "config.default.txt");
    for (const std::string& cle : artouste::app::clesConnues()) {
        INFO("clé absente du modèle réécrit : " << cle);
        REQUIRE(modeleRelu.find("\n" + cle + " ") != std::string::npos);
    }
    /* ... et la configuration personnelle a reçu les options qui lui manquaient. */
    REQUIRE(lireTout(dossier / "config.txt").find("verifier_maj 1") != std::string::npos);

    std::filesystem::remove_all(dossier);
}

TEST_CASE("modèle présent : jamais réécrit", "[config]") {
    /* Un modèle adapté volontairement (distribution personnalisée) doit être
       respecté : l'auto-réparation ne vaut que pour un fichier absent. */
    const auto dossier = preparerDossier("artouste_cfg_modele_perso", "terrain bordeaux\n");
    const std::string avant = lireTout(dossier / "config.default.txt");

    loadConfig(dossier / "config.txt");
    REQUIRE(lireTout(dossier / "config.default.txt") == avant);

    std::filesystem::remove_all(dossier);
}

namespace {

/* Clés portées par le vrai modèle livré avec le jeu (assets/config.default.txt),
   dont le chemin est passé par CMake. */
std::set<std::string> clesDuModele() {
    std::set<std::string> cles;
    std::ifstream in(ARTOUSTE_CONFIG_MODELE);
    REQUIRE(in); /* modèle introuvable : c'est le test lui-même qui est mal réglé */
    std::string ligne;
    while (std::getline(in, ligne)) {
        if (!ligne.empty() && ligne.back() == '\r') {
            ligne.pop_back();
        }
        const std::size_t debut = ligne.find_first_not_of(" \t");
        if (debut == std::string::npos || ligne[debut] == '#') {
            continue;
        }
        std::istringstream iss(ligne);
        std::string cle;
        iss >> cle;
        if (!cle.empty()) {
            cles.insert(cle);
        }
    }
    return cles;
}

} /* namespace */

TEST_CASE("option renommée : le réglage de l'utilisateur est conservé", "[config]") {
    /* Le jour où "arbres" deviendrait "vegetation", le fichier d'un ancien
       utilisateur doit suivre : sans cela sa clé serait rejetée comme inconnue et
       ses arbres coupés reviendraient, puisque le nouveau nom vaut 1 par défaut. */
    const std::map<std::string, std::string> renommages = {{"arbres", "vegetation"}};
    const auto path = writeTemp("artouste_cfg_renom",
                                "# mes reglages\n"
                                "terrain bordeaux\n"
                                "arbres 0\n"
                                "# fin\n");

    REQUIRE(artouste::app::renommerAnciennesCles(path, renommages) == 1);

    const std::string apres = lireTout(path);
    /* La valeur, la place et tout le reste du fichier sont intacts. */
    REQUIRE(apres == "# mes reglages\nterrain bordeaux\nvegetation 0\n# fin\n");

    effacerTemp(path);
}

TEST_CASE("option renommée : ni la mise en page ni les fins de ligne ne bougent", "[config]") {
    const std::map<std::string, std::string> renommages = {{"arbres", "vegetation"}};
    /* Indentation, espaces multiples et CRLF du Bloc-notes : rien ne doit changer
       hors le nom de la clé. */
    const auto path = writeTemp("artouste_cfg_renom_forme", "  arbres    0\r\nterrain ossau\r\n");

    REQUIRE(artouste::app::renommerAnciennesCles(path, renommages) == 1);
    REQUIRE(lireTout(path) == "  vegetation    0\r\nterrain ossau\r\n");

    effacerTemp(path);
}

TEST_CASE("option renommée déjà présente sous son nom actuel : pas de doublon", "[config]") {
    const std::map<std::string, std::string> renommages = {{"arbres", "vegetation"}};
    const auto path = writeTemp("artouste_cfg_renom_double", "arbres 0\nvegetation 1\n");

    REQUIRE(artouste::app::renommerAnciennesCles(path, renommages) == 1);

    const std::string apres = lireTout(path);
    /* L'ancienne ligne est neutralisée et expliquée, la ligne actuelle est intacte,
       et le fichier ne porte qu'une seule clé "vegetation" active. */
    REQUIRE(apres.find("\n# arbres 0") != std::string::npos);
    REQUIRE(apres.find("vegetation 1\n") != std::string::npos);
    REQUIRE(apres.find("Ancien nom") != std::string::npos);

    effacerTemp(path);
}

TEST_CASE("aucun renommage à faire : le fichier n'est pas réécrit", "[config]") {
    const std::map<std::string, std::string> renommages = {{"arbres", "vegetation"}};
    const auto path = writeTemp("artouste_cfg_renom_rien", "terrain bordeaux\n");
    const std::string avant = lireTout(path);

    REQUIRE(artouste::app::renommerAnciennesCles(path, renommages) == 0);
    REQUIRE(lireTout(path) == avant);
    /* Aucun fichier intermédiaire ne doit traîner. */
    REQUIRE_FALSE(std::filesystem::exists(path.string() + ".nouveau"));

    effacerTemp(path);
}

TEST_CASE("table de renommage cohérente avec les clés du chargeur", "[config]") {
    for (const auto& [ancien, actuel] : artouste::app::clesRenommees()) {
        INFO("renommage " << ancien << " -> " << actuel);
        /* Le nom d'arrivée doit exister... */
        REQUIRE(artouste::app::clesConnues().count(actuel) == 1);
        /* ... et l'ancien avoir vraiment disparu, sans quoi les deux noms
           vivraient en même temps et le renommage serait une perte de réglage. */
        REQUIRE(artouste::app::clesConnues().count(ancien) == 0);
        REQUIRE(clesDuModele().count(ancien) == 0);
    }
}

TEST_CASE("toute option du modèle est connue du chargeur", "[config]") {
    /* Sens modèle -> code. Une option décrite par le modèle mais que le chargeur
       ignore serait recopiée chez l'utilisateur, puis rejetée au lancement suivant
       avec un "clé inconnue" qui ressemble à un bug. */
    for (const std::string& cle : clesDuModele()) {
        INFO("clé du modèle absente de app::clesConnues() : " << cle);
        REQUIRE(artouste::app::clesConnues().count(cle) == 1);
    }
}

TEST_CASE("toute option du chargeur est documentée dans le modèle", "[config]") {
    /* Sens code -> modèle. Une option que le code sait lire mais que le modèle
       ne mentionne pas ne serait proposée à personne : ni aux nouveaux venus (qui
       reçoivent une copie du modèle), ni aux anciens (dont le fichier est complété
       depuis ce même modèle). */
    const std::set<std::string> modele = clesDuModele();
    for (const std::string& cle : artouste::app::clesConnues()) {
        INFO("clé du code absente de assets/config.default.txt : " << cle);
        REQUIRE(modele.count(cle) == 1);
    }
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
