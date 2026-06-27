/*
 * Application.hpp
 * Classe qui pilote tout le cycle de vie du simulateur : fenêtre GLFW,
 * contexte OpenGL, mise en place de la scène et boucle principale.
 * C'est le coeur du programme, autour duquel s'articulent le rendu, la
 * physique, les entrées et le son.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/DemoPilot.hpp"
#include "audio/AudioEngine.hpp"
#include "physics/FlightAssist.hpp"
#include "physics/FlightModel.hpp"
#include "render/Camera.hpp"
#include "ui/Hud.hpp"

#include <filesystem>
#include <memory>
#include <string>

struct GLFWwindow;

namespace artouste::render {
class Shader;
class Terrain;
class Buildings;
class HelicopterModel;
class LoadedHelicopter;
class Skybox;
class Mesh;
class Model;
}  /* namespace artouste::render */

namespace artouste::input {
class InputSystem;
}  /* namespace artouste::input */

namespace artouste::app {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    /* Initialise tout, déroule la boucle, nettoie. Renvoie un code de sortie. */
    int run();

private:
    bool initWindow();
    bool initGL();
    void initScene();
    void mainLoop();

    /* Charge (ou recharge) le terrain nommé : relief, bâtiments et position de
       départ. Réutilisable au runtime, notamment pour basculer sur Arcachon quand
       la démo est lancée alors qu'une autre carte est affichée. */
    void loadTerrain(const std::string& name);

    /* Auxiliaires de la boucle principale : chacun prend en charge une étape, pour
       que mainLoop reste un enchaînement lisible plutôt qu'un long bloc unique. */

    /* Commandes effectives de l'image : commandes du pilote automatique en mode
       démo, sinon commandes du pilote passées par le mode assisté. Met aussi à jour
       la vue et le HUD quand la démo les impose. */
    physics::Controls computeControls(const physics::Controls& rawInput, float frameDt, float t);

    /* Boutons et croix de la manette (hors commandes de vol) : vue, turbine, HUD,
       pause, reset, livrée, ainsi que les réponses Oui/Non des panneaux de
       confirmation. Neutralisés pendant la démo. */
    void handleActionButtons();

    /* Place la caméra selon la vue courante (poursuite, cockpit ou orbite) et gère
       le cut net au changement de vue. */
    void updateCamera(const mat4& base, const vec3& renderPos, float yaw, float t,
                      float frameDt);

    /* Module les boucles sonores (rotor, turbine, vent), déclenche le son de
       démarrage et calcule l'effet Doppler de la vue orbite. */
    void updateAudio(const physics::RigidBody& body, const physics::Controls& controls,
                     float airspeed, float turbineFraction, float rotorFraction, float frameDt);

    /* Fait tourner le rotor principal au prorata du régime, ou le ramène en douceur
       à sa position de parking quand la turbine est coupée. */
    void advanceRotor(float rotorFraction, float frameDt);

    void renderScene(const mat4& base, float rotorAngle, float rotorFraction,
                     float rudder = 0.0f, float cyclicLong = 0.0f, float cyclicLat = 0.0f,
                     float collective = 0.0f, float turbineFraction = 0.0f,
                     float timeSeconds = 0.0f);

    /* Heure du simulateur à l'instant t (s depuis le lancement), exprimée en
       secondes depuis minuit [0, 86400[. Part de l'heure locale du PC au lancement
       puis avance en temps réel (mode local) ou accéléré (mode accéléré). */
    float timeOfDaySeconds(float t) const;

    /* Direction (unitaire) du soleil à l'instant t (s depuis le lancement), selon
       le mode d'heure choisi (heure locale réelle ou temps accéléré). Source unique
       pour l'éclairage, le ciel et la caméra d'orbite solaire. */
    vec3 sunDirection(float t) const;

    /* Hélipads (départ + ceux du terrain) posés à plat au sol, dessinés avant
       l'appareil sans test de profondeur pour éviter le z-fighting au ras du sol. */
    void drawHelipads(const mat4& view, const mat4& proj, const vec3& lightDir);

    /* Ombre portée au sol (deux disques : fuselage et rotor), estompée avec
       l'altitude et posée au-dessus du relief pour ne pas le traverser en pente. */
    void drawGroundShadow(const mat4& base, float rotorFraction, const mat4& view,
                          const mat4& proj);

    /* Lueurs liées au moteur : strombo (flash rouge anti-collision au-dessus de la
       cabine, clignotant quand la turbine tourne) et tuyère (zone chaude jaune/rouge
       à la sortie de la turbine, d'intensité croissante avec le régime). */
    void drawEngineEffects(const mat4& base, float turbineFraction, float timeSeconds);

    /* Remplit les données instrumentales du HUD (altitude, vitesse, cap, régimes...)
       à partir de l'état physique courant. */
    void fillHud(ui::HudData& hud, const physics::RigidBody& body, const vec3& forward,
                 const physics::Controls& controls, float airspeed, float turbineFraction,
                 float rotorFraction, float t, float frameDt);

    /* Cherche l'hélipad le plus proche de heliPos (parmi m_terrain->helipads() et le
       pad de départ m_startPos), dans le rayon PAD_SEARCH_RADIUS_M. Remplit poseMonde
       avec la position monde du pad retenu et renvoie son nom (ou nullptr si aucun pad
       n'est dans le rayon). */
    const char* padPlusProche(const vec3& heliPos, vec3& poseMonde) const noexcept;

    /* Remplit le HUD de repérage : étiquettes des lieux remarquables projetées sur la
       scène et données de la minimap (position de l'appareil, points). */
    void buildNavHud(ui::HudData& hud, const vec3& heliPos, float headingDeg);
    void captureScreenshot(const std::filesystem::path& path);
    void onResize(int width, int height);

    /* Bascule la livrée Gendarmerie (partagée par la touche L et le bouton A). */
    void toggleGendarmerieLivery();

    /* Replace l'appareil au départ (appelé une fois la confirmation acceptée). */
    void resetToStart();

    /* Lance (ou relance en boucle) la démonstration automatique : appareil sur le
       pad de départ, turbine à froid puis démarrage rapide, et chorégraphie jouée
       par le pilote automatique m_demo. */
    void startDemo();

    /* Touche V : si la démo tourne, l'arrête ; sinon, affiche le panneau de
       confirmation avant de la lancer (réponse Oui/Non). */
    void toggleDemo();

    static void resizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    GLFWwindow* m_window = nullptr;
    int         m_width  = 1280;
    int         m_height = 720;

    render::Camera                            m_camera;
    std::filesystem::path                     m_assetsDir;     /* dossier des ressources (pour recharger un terrain au runtime) */
    std::string                               m_terrainName;   /* nom du terrain courant (sous-dossier de assets/terrain/) */
    vec3                                      m_startPos{0.0f, 0.0f, 0.0f};  /* centre du H (pad) */
    vec3                                      m_parkPos{0.0f, 0.0f, 0.0f};   /* origine de l'appareil au parking : décalée pour centrer le mât sur le H */
    std::unique_ptr<render::Shader>           m_shader;        /* géométrie à couleur (repli) */
    std::unique_ptr<render::Shader>           m_modelShader;   /* géométrie texturée (modèle) */
    std::unique_ptr<render::Shader>           m_terrainShader; /* terrain réel drapé d'orthophoto */
    std::unique_ptr<render::Shader>           m_seaShader;     /* plan de mer avec brume */
    std::unique_ptr<render::Shader>           m_skyShader;     /* ciel en dégradé */
    std::unique_ptr<render::Shader>           m_flatShader;    /* couleur unie (lueurs) */
    std::unique_ptr<render::Shader>           m_shadowShader;  /* ombre portée douce (dégradé) */
    std::unique_ptr<render::Shader>           m_buildingShader; /* bâtiments extrudés (éclairage + brume) */
    std::unique_ptr<render::Skybox>           m_sky;
    std::unique_ptr<render::Mesh>             m_shadowDisc;
    std::unique_ptr<render::Mesh>             m_glowSphere;    /* petite sphère lumineuse (strombo, tuyère) */
    std::unique_ptr<render::Mesh>             m_helipad;       /* marque au sol, repli procédural */
    std::unique_ptr<render::Model>           m_helipadModel;  /* hélipad texturé (modèle Blender) */
    std::unique_ptr<render::Mesh>             m_sea;           /* grand plan d'océan à l'horizon */
    std::unique_ptr<render::Terrain>          m_terrain;
    std::unique_ptr<render::Buildings>        m_buildings;     /* bâtiments 3D (BD TOPO extrudée) */
    std::unique_ptr<render::HelicopterModel>  m_helicopter;    /* repli procédural */
    std::unique_ptr<render::LoadedHelicopter> m_loadedHeli;    /* modèle FlightGear si présent */
    std::unique_ptr<input::InputSystem>       m_input;
    physics::FlightModel                      m_flight;
    physics::FlightAssist                     m_assist;        /* mode assisté : confort de pilotage (touche M / croix haut) */
    DemoPilot                                 m_demo;          /* pilote automatique du mode démo (inactif par défaut) */
    std::filesystem::path                     m_musicPath;     /* musique jouée pendant la démo (assets/music/demo.mp3) */
    std::string                               m_radioUrl;      /* URL du flux radio résolue au démarrage (touche K l'allume/coupe) */
    float                                     m_sunTimeScale   = 1.0f;  /* vitesse du temps : 1 = réel, 144 = jour en 10 min, 0 = figé */
    float                                     m_sunBaseSeconds = 0.0f;  /* heure locale du PC au lancement (s depuis minuit) */
    bool                                      m_demoWasActive = false;  /* pour couper la musique quand la démo s'arrête */
    bool                                      m_demoUserView = false;  /* en démo : l'utilisateur a repris la main sur la vue (touche C) */
    bool                                      m_demoUserHud  = false;  /* en démo : l'utilisateur a repris la main sur le HUD (touche H) */
    ui::Hud                                   m_hud;
    audio::AudioEngine                        m_audio;
    int                                       m_viewMode = 0;  /* 0 poursuite, 1 cockpit, 2 orbite */
    /* Origine de rendu (rendu relatif à la caméra) : on retranche cette position
       horizontale (X, Z ; Y laissé à 0 pour préserver les altitudes) de la caméra et
       de toutes les géométries avant de les confier au GPU. Les coordonnées près de la
       caméra restent ainsi petites, ce qui supprime le tremblement de précision float32
       en grandes coordonnées monde (terrain de plusieurs dizaines de km). Posée au
       début de renderScene, lue par les fonctions de rendu sol/effets. */
    vec3                                      m_renderOrigin{0.0f};
    /* Changement de vue : cut net (voir Camera::cut). On garde la vue précédente pour
       détecter le changement. */
    int                                       m_prevCamView = -1;  /* vue précédente (caméra), -1 au départ */
    float                                     m_orbitStart  = 0.0f;  /* instant d'entrée en vue orbite (démo : un tour complet) */
    ui::HudMode                               m_hudMode  = ui::HudMode::Corners;  /* coins -> superposé -> rien */
    bool                                      m_paused   = false;
    bool                                      m_confirmReset = false;  /* panneau Oui/Non avant un reset (touche X/R) */
    bool                                      m_confirmDemo  = false;  /* panneau Oui/Non avant de lancer la démo (touche V) */
    bool                                      m_gendarmerieLivery = true;  /* livrée Gendarmerie par défaut (touche L / bouton A) */
    float                                     m_rotorAngle = 0.0f;  /* angle du rotor principal (rad) : rotation au régime rotor, parking à l'arrêt */
    float                                     m_parkOffset = 0.0f;  /* décalage aléatoire de la position de parking (pale pas pile dans l'axe) */
    float                                     m_closingSpeed = 0.0f; /* vitesse de rapprochement caméra<->appareil lissée (effet Doppler, vue orbite) */
    physics::Turbine::State                   m_prevTurbineState = physics::Turbine::State::Arret;  /* pour déclencher le son de démarrage au bon moment */

    /* Aide à l'atterrissage (mode assisté) : état persistant entre les images. */
    float                                     m_scoreTimer  = 0.0f;   /* temps restant d'affichage du score (s) */
    float                                     m_lastScoreM  = -1.0f;  /* dernière distance au posé (m), -1 si aucun */
    bool                                      m_wasOnGround = false;  /* état sol de l'image précédente (anti-rebond) */
    bool                                      m_wasAirborne = false;  /* a volé depuis l'activation : évite un faux score au sol */
};

}  /* namespace artouste::app */
