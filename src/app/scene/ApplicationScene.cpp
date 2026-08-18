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
#include "render/Buildings.hpp"
#include "render/Clouds.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
#include "render/Vegetation.hpp"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <utility>

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
    bool arbresDefinis = false;
    bool arbres = true;
    bool batimentsDefinis = false;
    bool batiments = true;
    /* Tuiles de détail : éteindre sans effacer. Elles pèsent des gigaoctets sur
       le disque et une centaine de mégaoctets de mémoire vidéo ; sur une machine
       à l'étroit, on veut pouvoir renoncer aux secondes sans renoncer aux
       premiers, et les rallumer sans tout retélécharger. */
    bool tuilesDefinies = false;
    bool tuiles = true;
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
            options.arbres = valeurOui(valeur);
            options.arbresDefinis = true;
        } else if (cle == "batiments") {
            options.batiments = valeurOui(valeur);
            options.batimentsDefinis = true;
        } else if (cle == "tuiles") {
            options.tuiles = valeurOui(valeur);
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

std::filesystem::path Application::racineTuiles() const {
    /* La variable d'environnement prime sur la configuration : elle sert à
       essayer un autre disque le temps d'un lancement, sans rien réécrire. */
    if (const char* env = std::getenv("ARTOUSTE_TUILES"); env != nullptr && env[0] != '\0') {
        return env;
    }
    const std::filesystem::path demande = m_config.tilesDir;
    if (demande.empty() || demande.is_absolute()) {
        return demande;
    }
    /* Chemin relatif : compris depuis le dossier du jeu, celui qui contient
       "assets", et non depuis le répertoire courant, qui dépend de la façon dont
       le jeu a été lancé. m_assetsDir n'est pas encore renseigné au menu, d'où la
       localisation à la demande. */
    const std::filesystem::path assets = m_assetsDir.empty() ? resolveAssetDir() : m_assetsDir;
    return assets.parent_path() / demande;
}

Application::OptionsCarte
Application::optionsEffectives(const std::filesystem::path& dossierCarte) const {
    const OptionsLues lues = lireOptionsCarte(dossierCarte);
    OptionsCarte effectives;
    /* ARTOUSTE_NO_TREES garde le dernier mot : c'est l'interrupteur de secours,
       il doit couper les arbres même sur une carte qui les réclame. */
    effectives.arbres = (std::getenv("ARTOUSTE_NO_TREES") == nullptr) &&
                        (lues.arbresDefinis ? lues.arbres : m_treesEnabled);
    effectives.batiments = !lues.batimentsDefinis || lues.batiments;
    effectives.tuiles = !lues.tuilesDefinies || lues.tuiles;
    return effectives;
}

} /* namespace artouste::app */
