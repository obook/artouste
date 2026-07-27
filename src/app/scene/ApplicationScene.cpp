/*
 * ApplicationScene.cpp
 * Mise en place de la scène : initScene enchaîne le chargement des shaders et
 * maillages procéduraux (ApplicationSceneShaders.cpp) puis la lecture de la
 * configuration et le chargement du terrain (ApplicationSceneConfig.cpp). Ce
 * fichier garde aussi loadTerrain et applySunSchedule, réutilisés au runtime
 * (changement de carte, démo). La localisation du dossier des ressources vit
 * dans ApplicationAssets.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <GLFW/glfw3.h>
#include "render/Buildings.hpp"
#include "render/Clouds.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
#include "render/Vegetation.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace artouste::app {

namespace {

/*
 * Décalage maximal (rad) de la position de parking du rotor par rapport à l'axe.
 * Tiré au hasard à chaque lancement, pour que la pale ne soit pas figée pile dans
 * l'axe (plus naturel). Petit, pour rester loin de la sortie d'échappement.
 */
constexpr float ROTOR_PARK_JITTER = 0.26f; /* ~15 degrés */

/*
 * Options propres à une carte, lues dans son options.txt facultatif : même
 * mécanisme que ses autres fichiers optionnels (zombies.txt, hapi.txt,
 * exclusions.txt). Le gestionnaire de cartes les écrit, l'utilisateur peut les
 * éditer à la main.
 *
 * Elles sont PAR CARTE parce que les arbres comptent en montagne et les
 * bâtiments en ville, rarement les deux au même endroit, alors que la
 * configuration générale, elle, doit choisir une fois pour toutes. Une clé
 * absente laisse donc la valeur générale s'appliquer, et une carte sans ce
 * fichier se comporte exactement comme avant.
 */
struct OptionsLues {
    bool arbresDefinis    = false;
    bool arbres           = true;
    bool batimentsDefinis = false;
    bool batiments        = true;
    /* Tuiles de détail : éteindre sans effacer. Elles pèsent des gigaoctets sur
       le disque et une centaine de mégaoctets de mémoire vidéo ; sur une machine
       à l'étroit, on veut pouvoir renoncer aux secondes sans renoncer aux
       premiers, et les rallumer sans tout retélécharger. */
    bool tuilesDefinies = false;
    bool tuiles         = true;
};

[[nodiscard]] bool valeurOui(const std::string& valeur) {
    return !(valeur == "0" || valeur == "non" || valeur == "false");
}

[[nodiscard]] OptionsLues lireOptionsCarte(const std::filesystem::path& dir) {
    OptionsLues options;
    std::ifstream fichier(dir / "options.txt");
    if (!fichier) {
        return options;
    }
    std::string cle;
    while (fichier >> cle) {
        if (!cle.empty() && cle[0] == '#') {
            std::getline(fichier, cle);
            continue;
        }
        std::string valeur;
        if (!(fichier >> valeur)) {
            break;
        }
        if (cle == "arbres") {
            options.arbres        = valeurOui(valeur);
            options.arbresDefinis = true;
        } else if (cle == "batiments") {
            options.batiments        = valeurOui(valeur);
            options.batimentsDefinis = true;
        } else if (cle == "tuiles") {
            options.tuiles        = valeurOui(valeur);
            options.tuilesDefinies = true;
        }
        /* Clé inconnue : ignorée, un options.txt écrit par une version plus
           récente reste lisible. */
    }
    return options;
}

} /* namespace */

void Application::initScene() {
    /* Décalage de parking du rotor, tiré au hasard à chaque lancement : la pale ne
     * se range pas pile dans l'axe, ce qui est plus naturel. On y place aussi l'angle
     * de départ du rotor, pour qu'il soit déjà à cette position au lancement. */
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> jitter(-ROTOR_PARK_JITTER, ROTOR_PARK_JITTER);
    m_parkOffset = jitter(rng);
    m_rotorAngle = m_parkOffset;

    m_assetsDir = resolveAssetDir(); /* mémorisé pour recharger un terrain au runtime (démo) */
    m_musicPath = m_assetsDir / "music" / "demo.mp3"; /* musique de la démo (optionnelle) */

    initSceneShaders();
    initSceneConfig();
}

Application::OptionsCarte
Application::optionsEffectives(const std::filesystem::path& dossierCarte) const {
    const OptionsLues lues = lireOptionsCarte(dossierCarte);
    OptionsCarte      effectives;
    /* ARTOUSTE_NO_TREES garde le dernier mot : c'est l'interrupteur de secours,
       il doit couper les arbres même sur une carte qui les réclame. */
    effectives.arbres = (std::getenv("ARTOUSTE_NO_TREES") == nullptr) &&
                        (lues.arbresDefinis ? lues.arbres : m_treesEnabled);
    effectives.batiments = !lues.batimentsDefinis || lues.batiments;
    effectives.tuiles    = !lues.tuilesDefinies || lues.tuiles;
    return effectives;
}

void Application::loadTerrain(const std::string& name) {
    m_terrainName = name;
    std::printf("[scène] terrain : %s\n", name.c_str());
    const std::filesystem::path terrainDir = m_assetsDir / "terrain" / name;

    /* Options propres à cette carte (options.txt facultatif) : elles priment sur
       la configuration générale, qui ne peut pas savoir qu'on veut des arbres en
       montagne et des bâtiments en ville. Lues AVANT le terrain : la fenêtre de
       tuiles se décide à sa construction. Mémorisées, pour savoir au retour du
       menu si elles ont changé et s'il faut recharger. */
    m_optionsChargees       = optionsEffectives(terrainDir);
    const bool arbresIci    = m_optionsChargees.arbres;
    const bool batimentsIci = m_optionsChargees.batiments;
    const bool tuilesIci    = m_optionsChargees.tuiles;
    std::printf("[scène] options de la carte : arbres %s, bâtiments %s, tuiles %s\n",
                arbresIci ? "oui" : "non",
                batimentsIci ? "oui" : "non",
                tuilesIci ? "oui" : "non");

    /* Au tout premier chargement d'une carte, son orthophoto doit être
       compressée avant d'être mise en cache : une trentaine de secondes sur une
       carte fine, pendant lesquelles il faut montrer autre chose qu'une fenêtre
       figée. Les lancements suivants relisent le cache et n'appellent jamais ce
       rappel. Renvoyer faux annule la préparation : on le fait si l'utilisateur
       ferme la fenêtre, plutôt que de le retenir jusqu'au bout. */
    m_terrain = std::make_unique<render::Terrain>(
        terrainDir,
        [this](float fraction) {
            renderLoadingScreen("Préparation de la carte, une seule fois...", fraction);
            return glfwWindowShouldClose(m_window) == 0;
        },
        tuilesIci ? m_detailWindowPx : 0,
        m_reliefVertexBudget);

    /* Bâtiments 3D (BD TOPO extrudée) propres au terrain, posés sur le relief.
       Absents (fichier buildings.bin manquant) ou refusés par la carte : rien
       n'est dessiné. */
    m_buildings = batimentsIci ? std::make_unique<render::Buildings>(terrainDir, *m_terrain)
                               : nullptr;

    /* Végétation en billboards : arbres semés d'après l'orthophoto, posés sur le
       relief. Activée par défaut ; désactivable par la clé "arbres 0" de config.txt
       (et toujours désactivée si ARTOUSTE_NO_TREES est défini) -- voir m_treesEnabled,
       calculé dans initScene. Atlas de sprites partagé entre terrains. */
    if (arbresIci) {
        m_vegetation = std::make_unique<render::Vegetation>(
            terrainDir, *m_terrain, m_assetsDir / "vegetation" / "trees_atlas.png", m_treeBudget);
    } else {
        m_vegetation.reset();
    }

    /* Nuages en billboards (prototype) : couche de cumulus épars au-dessus du relief.
       Partagent la texture de bouffée entre terrains. */
    m_clouds = std::make_unique<render::Clouds>(*m_terrain, m_assetsDir / "clouds" / "puff.png");

    /*
     * Position de départ : posé à Fabrèges, le fond de vallée plat à l'entrée du
     * massif (lac de Fabrèges, station du téléphérique d'Artouste), face au relief.
     * Si le terrain réel est absent, on reste à l'origine sur le sol plat de repli.
     */
    if (m_terrain->textured()) {
        /* Point de départ : on privilégie les hélipads réels (helipads.txt) comme aire
           de poser. Le repère du terrain (start_x/start_z, sinon Fabrèges par défaut)
           ne sert qu'à choisir LEQUEL : on cale le départ sur l'hélipad le plus proche
           de ce repère. helipads.txt reste ainsi la seule source de la position exacte,
           sans la dupliquer dans terrain.txt. Sans hélipad, on garde le repère brut.
           Négatif en X = ouest, positif en Z = sud. */
        float START_X = m_terrain->hasStart() ? m_terrain->startX() : 158.0f;
        float START_Z = m_terrain->hasStart() ? m_terrain->startZ() : -3119.6f;
        const float hintX = START_X;
        const float hintZ = START_Z;
        float bestD2 = -1.0f; /* sentinelle : aucun hélipad encore retenu */
        m_homeStation.clear();
        for (const render::Landmark& pad : m_terrain->helipads()) {
            float px = 0.0f, pz = 0.0f;
            m_terrain->worldAt(pad.lon, pad.lat, px, pz);
            const float d2 = (px - hintX) * (px - hintX) + (pz - hintZ) * (pz - hintZ);
            if (bestD2 < 0.0f || d2 < bestD2) {
                bestD2 = d2;
                START_X = px;
                START_Z = pz;
                m_homeStation = pad.name; /* station d'origine = hélipad de départ */
            }
        }
        const float ground = m_terrain->heightAt(START_X, START_Z);
        m_startPos = vec3{START_X, ground, START_Z};
        /* L'appareil se gare mât rotor centré sur le H : son origine (que la
           physique place) est donc reculée de ROTOR_FORWARD_OFFSET le long de l'axe
           de départ, donné par le cap initial de la carte (clé start_heading de
           terrain.txt ; 90 = est par défaut, l'orientation identité). */
        const float capRad = glm::radians(m_terrain->startHeadingDeg());
        const vec3 avant{std::sin(capRad), 0.0f, -std::cos(capRad)};
        const float parkX = START_X - avant.x * render::LoadedHelicopter::ROTOR_FORWARD_OFFSET;
        const float parkZ = START_Z - avant.z * render::LoadedHelicopter::ROTOR_FORWARD_OFFSET;
        m_parkPos = vec3{parkX, m_terrain->heightAt(parkX, parkZ), parkZ};
        m_flight.reset(m_parkPos, m_terrain->startHeadingDeg());
    }
}

void Application::applySunSchedule() {
    /* Cycle jour/nuit : vitesse du temps (clé `sun_time_scale` de la config, 1 =
       temps réel). m_sunBaseSeconds est l'heure d'origine du soleil (s depuis
       minuit), voir Application::sunDirection :
         - en temps réel (échelle 1) on part de l'heure locale du PC ;
         - sinon (temps accéléré ou figé) on part de midi, pour démarrer sur une belle
           lumière plutôt qu'en pleine nuit selon l'heure du PC. Avec une échelle nulle,
           le soleil reste donc figé à midi. */
    m_sunTimeScale = m_config.sunTimeScale;
    if (m_sunTimeScale == 1.0f) {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        m_sunBaseSeconds = static_cast<float>(local.tm_hour) * 3600.0f +
                           static_cast<float>(local.tm_min) * 60.0f +
                           static_cast<float>(local.tm_sec);
        std::printf("[scène] cycle jour/nuit : temps réel, heure locale au lancement %02d:%02d.\n",
                    local.tm_hour,
                    local.tm_min);
    } else {
        constexpr float NOON = 12.0f * 3600.0f; /* midi */
        m_sunBaseSeconds = NOON;
        if (m_sunTimeScale == 0.0f) {
            std::printf("[scène] cycle jour/nuit : temps figé à midi.\n");
        } else {
            std::printf("[scène] cycle jour/nuit : temps accéléré (x%g), départ à midi.\n",
                        static_cast<double>(m_sunTimeScale));
        }
    }

    /* Arène dédiée au mode zombie (Happy DeathHour, zombie_only.txt) : nuit figée
       plutôt que le réglage ci-dessus -- ambiance de combat nocturne constante,
       sans cycle jour/nuit dans une arène fermée. 19h00 place la lune à ~14°
       au-dessus de l'horizon dans l'axe du cap de départ (voir sunDirection,
       ApplicationSun.cpp) : bien visible depuis le cockpit sans lever la tête
       (vérifié par capture -- au-delà de ~20°, la vue cockpit, inclinée vers le
       bas, la sort du cadre). Testé sur m_combat.active() : sans ce garde-fou,
       quitter cette arène pour une carte normale garderait le temps figé
       indéfiniment, faute d'être jamais réévalué depuis la config. */
    if (m_combat.active() &&
        std::filesystem::exists(m_assetsDir / "terrain" / m_terrainName / "zombie_only.txt")) {
        constexpr float NIGHT_HOUR_S = 19.0f * 3600.0f;
        m_sunBaseSeconds = NIGHT_HOUR_S;
        m_sunTimeScale = 0.0f;
    }
}

} /* namespace artouste::app */
