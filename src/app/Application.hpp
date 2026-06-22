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
    void renderScene(const mat4& base, float rotorAngle, float rotorFraction,
                     float rudder = 0.0f, float cyclicLong = 0.0f, float cyclicLat = 0.0f,
                     float collective = 0.0f, float turbineFraction = 0.0f,
                     float timeSeconds = 0.0f);

    /* Lueurs liées au moteur : strombo (flash rouge anti-collision au-dessus de la
       cabine, clignotant quand la turbine tourne) et tuyère (zone chaude jaune/rouge
       à la sortie de la turbine, d'intensité croissante avec le régime). */
    void drawEngineEffects(const mat4& base, float turbineFraction, float timeSeconds);

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
    bool                                      m_demoWasActive = false;  /* pour couper la musique quand la démo s'arrête */
    ui::Hud                                   m_hud;
    audio::AudioEngine                        m_audio;
    int                                       m_viewMode = 0;  /* 0 poursuite, 1 cockpit, 2 orbite */
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
};

}  /* namespace artouste::app */
