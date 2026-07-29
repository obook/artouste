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

#include "app/Config.hpp"
#include "app/DemoPilot.hpp"
#include "app/LandingAutopilot.hpp"
#include "app/MiseAJour.hpp"
#include "app/SouffleRotor.hpp"
#include "app/cartes/FabriqueTuiles.hpp"
#include "app/combat/CombatMode.hpp"
#include "audio/AudioEngine.hpp"
#include "physics/FlightAssist.hpp"
#include "physics/FlightModel.hpp"
#include "render/Camera.hpp"
#include "render/Livery.hpp"
#include "ui/Hud.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;
struct GLFWmonitor;

namespace artouste::render {
class Shader;
class Terrain;
class Buildings;
class Vegetation;
class Clouds;
class HelicopterModel;
class LoadedHelicopter;
class Skybox;
class Mesh;
class Model;
class Texture;
class SkinnedZombies;
class ExplosionFx;
class Projectiles;
class ZombieEyes;
class SouffleFx;
} /* namespace artouste::render */

namespace artouste::input {
class InputSystem;
} /* namespace artouste::input */

namespace artouste::app {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /* Initialise tout, déroule la boucle, nettoie. Renvoie un code de sortie. */
    int run();

private:
    /* --- Cycle de vie et initialisation -------------------------------------- */

    /* Vérifie que le dossier des ressources ("assets") est bien présent avant
       d'ouvrir la fenêtre. En cas d'absence (typiquement un exe lancé depuis
       l'intérieur du zip, sans les ressources à côté), affiche un message natif
       expliquant qu'il faut d'abord extraire l'archive, puis renvoie false. */
    bool assetsDisponibles();

    bool initWindow();
    bool initGL();
    void initScene();

    /* Charge les shaders, les maillages procéduraux (disque, sphère, hélipad,
       jupe) et les modèles associés (zombies, projectiles, explosions,
       hélipad texturé), plus les textures de détail. Appelée par initScene.
       Définie dans ApplicationSceneShaders.cpp. */
    void initSceneShaders();

    /* Lit la configuration et les variables d'environnement ARTOUSTE_* (démo,
       arbres, terrain, turbine, radio), charge le terrain choisi et démarre
       la démo/le combat demandés. Appelée par initScene. Définie dans
       ApplicationSceneConfig.cpp. */
    void initSceneConfig();
    /* Boucle de vol. Renvoie true si l'utilisateur a demandé le retour au menu (touche
       Échap), false s'il a fermé la fenêtre (on quitte alors l'application). */
    bool mainLoop();

    /* Applique un nouveau choix du menu à une session déjà en cours (retour au menu par
       Échap) : recharge le terrain s'il a changé (sinon repose l'appareil au parking),
       règle l'état de la turbine, et repart d'un état neutre (ni démo, ni pause). */
    void applyMenuSession();

    /* --- Menu de démarrage ---------------------------------------------------- */

    /* Une carte proposée au menu : nom du sous-dossier (valeur pour le terrain) et
       libellé lisible affiché à l'utilisateur. Définie et peuplée dans
       ApplicationMenuMaps.cpp, consommée par runStartupMenu (ApplicationMenu.cpp). */
    struct MapEntry {
        std::string dir;   /* nom du sous-dossier de assets/terrain (= nom du terrain) */
        std::string title; /* libellé lisible, tiré de la première ligne de terrain.txt */
        bool zombieCapable = false; /* présence de zombies.txt : mode zombie proposé */
        /* Présence de zombie_only.txt : carte dédiée au mode zombie (ex. dax-arene),
           sans autre usage -- lancer normalement (Démarrer/Entrée/A) suffit à
           démarrer le combat, pas besoin du bouton "Mode Zombie" ni de Z/LB. */
        bool zombieOnly = false;
    };

    /* Recense les cartes : chaque sous-dossier de assets/terrain contenant un
       terrain.txt, trié par nom pour un ordre stable (la première est le choix par
       défaut), sauf les cartes dédiées au mode zombie qui passent systématiquement
       en dernier. Définie dans ApplicationMenuMaps.cpp. */
    static std::vector<MapEntry> recenserCartes(const std::filesystem::path& assets);

    /* Ce qu'une carte occupe sur le disque, et ce qu'elle affiche. Rempli par
       inventorierCartes, consommé par le gestionnaire de cartes
       (ApplicationMenuCartes.cpp). */
    struct EtatCarte {
        std::string           dir;
        std::string           titre;
        std::filesystem::path dossier;
        std::filesystem::path dossierTuiles;  /* vide si la carte n'a pas de tuiles */
        std::uintmax_t        octetsSocle     = 0;
        std::uintmax_t        octetsBatiments = 0;
        std::uintmax_t        octetsTuiles    = 0;
        /* Finesse du jeu de tuiles en place, en mètres par pixel (0 s'il n'y en a
           pas), et ce que des tuiles peuvent apporter à cette carte. Comparer les
           deux est la SEULE façon de savoir si ces mégaoctets servent à quelque
           chose : le moteur écarte au chargement un jeu qui n'est pas plus fin
           que l'orthophoto d'ensemble. */
        float           finesseTuiles = 0.0f;
        cartes::Interet interet;
        /* Fabrication interrompue : le dossier porte un index complet mais n'a
           reçu qu'une partie de ses tuiles. Sans ces trois champs, l'écran
           annoncerait un jeu entier là où il n'y a qu'un coin de carte. */
        bool tuilesInachevees = false;
        int  tuilesPresentes  = 0;  /* fichiers de tuiles réellement sur le disque */
        int  tuilesAttendues  = 0;  /* colonnes x rangées annoncées par l'index */
        /* Options effectives de la carte. Chacune est accompagnée de son drapeau
           "défini" : faux tant que la carte ne tranche pas elle-même, auquel cas
           la valeur vient de la configuration générale. On n'écrit dans le
           options.txt d'une carte que ce qui a été explicitement réglé, sinon
           toucher un seul réglage figerait les deux autres et soustrairait la
           carte à la configuration générale sans le dire. */
        bool arbres          = true;
        bool arbresDefini    = false;
        bool batiments       = true;
        bool batimentsDefini = false;
        /* Tuiles utilisées ou seulement conservées : les éteindre libère la
           mémoire vidéo sans rien effacer du disque. */
        bool tuiles       = true;
        bool tuilesDefinie = false;
    };

    /* Recense les cartes du dossier de ressources donné et mesure ce qu'elles
       occupent. Le chemin est passé en argument parce que le gestionnaire s'ouvre
       depuis le menu, avant que m_assetsDir ne soit renseigné. Parcourt des
       milliers de fichiers sur une carte tuilée : à appeler à l'ouverture de
       l'écran, pas à chaque image. */
    std::vector<EtatCarte> inventorierCartes(const std::filesystem::path& assets);

    /* Gestionnaire de cartes : montre l'occupation disque de chaque carte, bascule
       ses arbres et ses bâtiments, et libère la place prise par ses tuiles. Rend
       la main au menu de démarrage. Défini dans ApplicationMenuCartes.cpp. */
    void runGestionnaireCartes();

    /* Menu de démarrage affiché dans la fenêtre (ImGui) : choix de la carte et du
       démarrage immédiat de la turbine, à la place de l'ancien launch.bat (bloqué par
       le Contrôle intelligent des applications de Windows). Les choix sont déposés dans
       m_menuTerrain / m_menuTurbine, lus ensuite par initScene(). Renvoie false si
       l'utilisateur ferme la fenêtre sans lancer (on quitte alors sans charger la scène).
       Non appelé en mode capture ni quand la carte est déjà imposée par une variable
       d'environnement. */
    bool runStartupMenu();

    /* Affiche un message centré ("Chargement...") puis force son rendu à l'écran
       (swap buffer immédiat) avant de lancer une étape bloquante (chargement de
       terrain, du modèle 3D...) : sans cela, la fenêtre reste figée sur la dernière
       image du menu pendant 3-4 s, sans retour visuel. Appelle glfwPollEvents()
       pour éviter que l'OS ne marque la fenêtre "ne répond pas". Nécessite que
       m_hud soit déjà initialisé (contexte ImGui). Définie dans ApplicationMenu.cpp. */
    /* Écran d'attente affiché avant un chargement bloquant. progression < 0
       n'affiche que le message ; entre 0 et 1, une barre s'y ajoute (voir la
       préparation de l'orthophoto dans TextureCache.hpp). */
    void renderLoadingScreen(const char* message, float progression = -1.0f);

    /* --- Ressources et fenêtre ------------------------------------------------ */

    /* Localise le dossier des ressources "assets" (variable d'environnement, puis à
       côté de l'exécutable, puis chemin de compilation). Statique : utilisable avant
       toute initialisation, notamment par le menu de démarrage. Définie dans
       ApplicationScene.cpp. */
    static std::filesystem::path resolveAssetDir();

    /* Plein écran sans bordure (résolution native du bureau, sans changement de mode
       vidéo) ou fenêtré. Masque le curseur en plein écran, le rétablit en fenêtré.
       toggleFullscreen est reliée à la touche F ; le lancement se fait en plein écran. */
    void setFullscreen(bool on);
    void toggleFullscreen();

    /* Moniteur sur lequel se met le plein écran : celui qui contient la fenêtre au
       moment de l'appel (le gestionnaire de fenêtres l'ouvre sur l'écran actif, celui
       du curseur), plutôt que le "principal" GLFW qui peut être un autre écran. Repli
       sur le moniteur principal si la position n'est pas exploitable. */
    GLFWmonitor* monitorForWindow() const;

    /* --- Terrain et cycle jour/nuit -------------------------------------------- */

    /* Options propres à une carte (fichier options.txt facultatif), une fois
       combinées à la configuration générale : ce que le moteur applique
       réellement à cette carte-là. */
    struct OptionsCarte {
        bool arbres    = true;
        bool batiments = true;
        bool tuiles    = true;
        [[nodiscard]] bool operator==(const OptionsCarte&) const = default;
    };

    /* Racine où chercher les jeux de tuiles : clé "tuiles_dossier" de la
       configuration, surchargée par ARTOUSTE_TUILES, et rendue absolue depuis le
       dossier du jeu si elle est relative. Utilisable dès le menu, avant même le
       chargement de la scène. Définie dans ApplicationScene.cpp. */
    [[nodiscard]] std::filesystem::path racineTuiles() const;

    /* Lit les options effectives d'une carte. Définie dans ApplicationScene.cpp. */
    [[nodiscard]] OptionsCarte optionsEffectives(const std::filesystem::path& dossierCarte) const;

    /* Charge (ou recharge) le terrain nommé : relief, bâtiments et position de
       départ. Réutilisable au runtime, notamment pour basculer sur Arcachon quand
       la démo est lancée alors qu'une autre carte est affichée. */
    void loadTerrain(const std::string& name);

    /* (Re)règle l'heure/vitesse du soleil (m_sunTimeScale/m_sunBaseSeconds) : la
       clé soleil_vitesse de la config par défaut, sauf sur une arène dédiée au
       mode zombie (zombie_only.txt du terrain courant, ex. Happy DeathHour) où la
       nuit est figée en permanence. Appelée par initScene ET applyMenuSession
       (pas seulement au premier lancement) : sans quoi une session qui visite une
       telle arène puis change de carte garde le temps figé indéfiniment sur les
       cartes suivantes -- le réglage n'était jamais réévalué à partir de la
       config lors d'un changement de carte en cours de session. */
    void applySunSchedule();

    /* --- Boucle principale -----------------------------------------------------
     * Auxiliaires de la boucle principale : chacun prend en charge une étape, pour
     * que mainLoop reste un enchaînement lisible plutôt qu'un long bloc unique.
     * ------------------------------------------------------------------------- */

    /* Commandes effectives de l'image : commandes du pilote automatique en mode
       démo, sinon commandes du pilote passées par le mode assisté. Met aussi à jour
       la vue et le HUD quand la démo les impose. Définie dans
       ApplicationControls.cpp. */
    physics::Controls computeControls(const physics::Controls& rawInput, float frameDt, float t);

    /* Boutons et croix de la manette (hors commandes de vol) : vue, turbine, HUD,
       pause, reset, livrée, ainsi que les réponses Oui/Non des panneaux de
       confirmation. Neutralisés pendant la démo. Définie dans
       ApplicationInputButtons.cpp. */
    void handleActionButtons();

    /* Place la caméra selon la vue courante (poursuite, cockpit ou orbite) et gère
       le cut net au changement de vue. Définie dans ApplicationCameraAudio.cpp. */
    void updateCamera(const mat4& base, const vec3& renderPos, float yaw, float t, float frameDt);

    /* Module les boucles sonores (rotor, turbine, vent), déclenche le son de
       démarrage et calcule l'effet Doppler de la vue orbite. Définie dans
       ApplicationCameraAudio.cpp. */
    void updateAudio(const physics::RigidBody& body,
                     const physics::Controls& controls,
                     float airspeed,
                     float turbineFraction,
                     float rotorFraction,
                     float frameDt);

    /* Fait tourner le rotor principal au prorata du régime, ou le ramène en douceur
       à sa position de parking quand la turbine est coupée. */
    void advanceRotor(float rotorFraction, float frameDt);

    /* Émet un message radio (voix de synthèse Flite + sous-titre) 2 s après que la
       turbine atteint son plein régime. Se réarme quand la turbine redescend. */
    void updateRadioMessage(float turbineFraction, float frameDt);

    /* Réarme l'annonce de la tour pour la session qui commence. Le réarmement
       automatique attend que la turbine redescende sous la moitié du régime, ce
       qui n'arrive jamais entre deux sessions lancées turbine chaude (mode
       zombie) : sans cet appel, seule la première du processus est annoncée. */
    void resetRadioMessage() noexcept;

    /* --- Rendu d'une image ------------------------------------------------------ */

    void renderScene(const mat4& base,
                     float rotorAngle,
                     float rotorFraction,
                     float rudder = 0.0f,
                     float cyclicLong = 0.0f,
                     float cyclicLat = 0.0f,
                     float collective = 0.0f,
                     float turbineFraction = 0.0f,
                     float timeSeconds = 0.0f);

    /* Grandeurs communes à toutes les étapes de renderScene, calculées une fois
       (position/matrices relatives à la caméra, brume) et passées aux
       sous-méthodes ci-dessous. Voir renderScene pour leur calcul. */
    struct RenderContext {
        vec3 lightDir;
        mat4 proj;
        mat4 view;
        mat4 toRel;     /* translation -m_renderOrigin, appliquée aux modèles */
        vec3 camPosRel; /* position caméra, relative à m_renderOrigin */
        vec3 fogColor;  /* couleur de brume, assombrie la nuit */
    };

    /* Ciel en dégradé et plan de mer. Définie dans ApplicationRenderWorld.cpp. */
    void renderSkyAndSea(const RenderContext& ctx, float timeSeconds);

    /* Terrain texturé (ou repli à couleurs de sommets) et bâtiments 3D extrudés.
       Définie dans ApplicationRenderWorld.cpp. */
    void renderTerrainAndBuildings(const RenderContext& ctx);

    /* Végétation en billboards et nuages. Définie dans
       ApplicationRenderWorld.cpp. */
    void renderVegetationAndClouds(const RenderContext& ctx);

    /* Mode zombie : personnages skinnés, boulettes toxiques et explosions 3D
       des roquettes. Ne dessine rien hors combat. Définie dans
       ApplicationRenderActors.cpp. */
    void renderCombatEntities(const RenderContext& ctx, float timeSeconds);

    /* Hélicoptère : modèle FlightGear texturé s'il est chargé (avec ses
       instruments de cockpit dérivés de 'base'), sinon version procédurale.
       Définie dans ApplicationRenderActors.cpp. */
    void renderHelicopter(const RenderContext& ctx,
                          const mat4& base,
                          float rotorAngle,
                          float rudder,
                          float cyclicLong,
                          float cyclicLat,
                          float collective,
                          float rotorFraction);

    /* Heure du simulateur à l'instant t (s depuis le lancement), exprimée en
       secondes depuis minuit [0, 86400[. Part de l'heure locale du PC au lancement
       puis avance en temps réel (mode local) ou accéléré (mode accéléré). */
    float timeOfDaySeconds(float t) const;

    /* Direction (unitaire) du soleil à l'instant t (s depuis le lancement), selon
       le mode d'heure choisi (heure locale réelle ou temps accéléré). Source unique
       pour l'éclairage, le ciel et la caméra d'orbite solaire. */
    vec3 sunDirection(float t) const;

    /* --- Décalques au sol et lueurs du moteur ----------------------------------- */

    /* Hélipads (départ + ceux du terrain) posés à plat au sol, dessinés avant
       l'appareil sans test de profondeur pour éviter le z-fighting au ras du sol.
       Définie dans ApplicationGround.cpp. */
    void drawHelipads(const mat4& view, const mat4& proj, const vec3& lightDir);

    /* Balises HAPI du terrain (indicateur de pente d'approche) : une petite
       lueur au sol par balise, recolorée à chaque image selon la position de
       l'appareil par rapport à la pente d'approche visée.
       Définie dans ApplicationGroundHapi.cpp. */
    void drawHapi(const mat4& view, const mat4& proj, float timeSeconds);

    /* Ombre portée au sol (deux disques : fuselage et rotor), estompée avec
       l'altitude et posée au-dessus du relief pour ne pas le traverser en pente.
       sunDir est la direction du soleil (vers le soleil) : l'ombre est décalée à
       l'opposé et étirée quand le soleil est bas.
       Définie dans ApplicationGroundShadow.cpp. */
    void drawGroundShadow(const mat4& base,
                          float rotorFraction,
                          const mat4& view,
                          const mat4& proj,
                          const vec3& sunDir);

    /* Traces de brûlure au sol laissées par les impacts de roquettes (mode
       zombie) : décalques sombres qui s'estompent en ~45 s, dessinés avec le
       maillage et le shader d'ombre (voir CombatMode::scorches).
       Définie dans ApplicationGround.cpp. */
    void drawScorchMarks(const mat4& view, const mat4& proj);

    /* Lueurs liées au moteur : strombo (flash rouge anti-collision au-dessus de la
       cabine, clignotant quand la turbine tourne) et tuyère (zone chaude jaune/rouge
       à la sortie de la turbine, d'intensité croissante avec le régime). */
    void drawEngineEffects(const mat4& base, float turbineFraction, float timeSeconds);

    /* Souffle rotor : avance le nuage de poussière soulevé près du sol (émission
       sous l'axe du mât, que 'base' situe dans le monde) puis le dessine. Un pas
       de temps nul fige le nuage sans le faire disparaître (pause).
       Définies dans ApplicationRenderEffects.cpp. */
    void updateSouffle(const mat4& base, float rotorFraction, float collective, float dt);
    void drawSouffle(const RenderContext& ctx);

    /* --- HUD -------------------------------------------------------------------- */

    /* Remplit les données instrumentales du HUD (altitude, vitesse, cap, régimes...)
       à partir de l'état physique courant. */
    void fillHud(ui::HudData& hud,
                 const physics::RigidBody& body,
                 const vec3& forward,
                 const physics::Controls& controls,
                 float airspeed,
                 float turbineFraction,
                 float rotorFraction,
                 float t,
                 float frameDt);

    /* Aide à l'atterrissage : réticule de centrage sur le pad le plus proche et
       score au posé (voir hud.padGuidance). Appelée par fillHud. Définie dans
       ApplicationHudLanding.cpp. */
    void updateLandingAid(ui::HudData& hud,
                          const physics::RigidBody& body,
                          const vec3& forward,
                          float frameDt);

    /* Cherche l'hélipad le plus proche de heliPos (parmi m_terrain->helipads() et le
       pad de départ m_startPos), dans le rayon PAD_SEARCH_RADIUS_M. Remplit poseMonde
       avec la position monde du pad retenu et renvoie son nom (ou nullptr si aucun pad
       n'est dans le rayon). Définie dans ApplicationHudLanding.cpp. */
    const char* padPlusProche(const vec3& heliPos, vec3& poseMonde) const noexcept;

    /* Remplit le HUD de repérage : étiquettes des lieux remarquables projetées sur la
       scène et données de la minimap (position de l'appareil, points). */
    void buildNavHud(ui::HudData& hud, const vec3& heliPos, float headingDeg, float timeSeconds);
    void captureScreenshot(const std::filesystem::path& path);
    void onResize(int width, int height);

    /* --- Actions diverses (livrée, reset, démo, atterrissage automatique) ------- */

    /* Fait défiler la livrée (partagée par la touche L et le bouton A) : origine ->
       Gendarmerie -> armée de terre -> origine. */
    void cycleLivery();

    /* Replace l'appareil au départ (appelé une fois la confirmation acceptée). */
    void resetToStart();

    /* Lance (ou relance en boucle) la démonstration automatique : appareil sur le
       pad de départ, turbine à froid puis démarrage rapide, et chorégraphie jouée
       par le pilote automatique m_demo. */
    void startDemo();

    /* Bascule l'atterrissage automatique (touche J / RB) : engage m_autoland
       vers l'hélipad le plus proche s'il est inactif, le désengage sinon (le pilote
       reprend la main). Sans effet en démo (pas de call site pendant la démo). */
    void toggleAutoland();

    /* --- Callbacks statiques GLFW ------------------------------------------------ */

    static void resizeCallback(GLFWwindow* window, int width, int height);
    /* Définie dans ApplicationInputKeyboard.cpp. */
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    /* ============================================================================
     * Membres, regroupés par thème : fenêtre, caméra et rendu du monde, combat,
     * hélicoptère, entrées et physique, audio et radio, temps et soleil, HUD et
     * vues, menu et configuration, état de session, livrée et rotor, aide à
     * l'atterrissage.
     * ============================================================================ */

    /* --- Fenêtre et système ------------------------------------------------------ */

    GLFWwindow* m_window = nullptr;
    int m_width = 1280;
    int m_height = 720;

    /* --- Monde : caméra, shaders, maillages et décor ------------------------------ */

    render::Camera m_camera;
    std::filesystem::path
        m_assetsDir;           /* dossier des ressources (pour recharger un terrain au runtime) */
    std::string m_terrainName; /* nom du terrain courant (sous-dossier de assets/terrain/) */
    vec3 m_startPos{0.0f, 0.0f, 0.0f}; /* centre du H (pad) */
    vec3 m_parkPos{
        0.0f,
        0.0f,
        0.0f}; /* origine de l'appareil au parking : décalée pour centrer le mât sur le H */
    std::unique_ptr<render::Shader> m_shader;         /* géométrie à couleur (repli) */
    std::unique_ptr<render::Shader> m_modelShader;    /* géométrie texturée (modèle) */
    std::unique_ptr<render::Shader> m_terrainShader;  /* terrain réel drapé d'orthophoto */
    std::unique_ptr<render::Shader> m_seaShader;      /* plan de mer avec brume */
    std::unique_ptr<render::Shader> m_skyShader;      /* ciel en dégradé */
    std::unique_ptr<render::Shader> m_flatShader;     /* couleur unie (lueurs) */
    std::unique_ptr<render::Shader> m_shadowShader;   /* ombre portée douce (dégradé) */
    std::unique_ptr<render::Shader> m_buildingShader; /* bâtiments extrudés (éclairage + brume) */
    std::unique_ptr<render::Shader> m_vegetationShader; /* arbres en billboards instanciés */
    std::unique_ptr<render::Shader> m_cloudShader;      /* nuages en billboards (mélange alpha) */
    std::unique_ptr<render::Shader> m_zombieShader; /* mode zombie : modèle 3D skinné instancié */
    std::unique_ptr<render::Shader>
        m_projectileShader; /* mode zombie : boulettes toxiques (billboard) */
    std::unique_ptr<render::Shader>
        m_explosionShader; /* mode zombie : explosion 3D animée (émissive) */
    std::unique_ptr<render::Shader>
        m_zombieEyesShader; /* mode zombie : lueur des yeux (billboard additif) */
    std::unique_ptr<render::Shader>
        m_souffleShader; /* souffle rotor : poussière en billboards instanciés */
    std::unique_ptr<render::Skybox> m_sky;
    std::unique_ptr<render::Mesh> m_shadowDisc;
    std::unique_ptr<render::Mesh> m_glowSphere;    /* petite sphère lumineuse (strombo, tuyère) */
    std::unique_ptr<render::Mesh> m_helipad;       /* marque au sol, repli procédural */
    std::unique_ptr<render::Model> m_helipadModel; /* hélipad texturé (modèle Blender) */
    std::unique_ptr<render::Mesh> m_padSkirt;      /* jupe sous le disque (pad perché) */
    std::unique_ptr<render::Texture> m_loadingImage;  /* fond de l'écran d'attente */
    std::unique_ptr<render::Texture> m_terrainDetail; /* grain rocheux du terrain (unité 1) */
    std::unique_ptr<render::Texture> m_buildingFacade; /* façade tuilée des bâtiments (unité 0) */
    std::unique_ptr<render::Mesh> m_sea;               /* grand plan d'océan à l'horizon */
    std::unique_ptr<render::Terrain> m_terrain;
    std::unique_ptr<render::Buildings> m_buildings;   /* bâtiments 3D (BD TOPO extrudée) */
    std::unique_ptr<render::Vegetation> m_vegetation; /* arbres en billboards (prototype) */
    std::unique_ptr<render::Clouds> m_clouds;         /* nuages en billboards (prototype) */

    /* --- Souffle rotor -------------------------------------------------------------
       Poussière soulevée au ras du sol : la simulation (app::SouffleRotor) est
       indépendante du rendu (render::SouffleFx), qui n'existe que si l'effet est
       activé (clé "souffle" de la configuration). */

    SouffleRotor m_souffle;
    std::unique_ptr<render::SouffleFx> m_souffleFx;
    bool m_souffleEnabled = true;

    /* --- Mode zombie -------------------------------------------------------------- */

    std::unique_ptr<render::SkinnedZombies>
        m_zombiesRender; /* mode zombie : pack skinné animé, chargé une fois */
    std::unique_ptr<render::Projectiles> m_projectilesRender; /* mode zombie : boulettes toxiques */
    std::unique_ptr<render::ZombieEyes>
        m_zombieEyesRender; /* mode zombie : lueur des yeux, deux par zombie */
    std::unique_ptr<render::ExplosionFx>
        m_explosionFx;   /* mode zombie : explosions 3D à l'impact des roquettes */
    CombatMode m_combat; /* mode zombie : horde et état de session */
    /* --- Hélicoptère ---------------------------------------------------------------- */

    std::unique_ptr<render::HelicopterModel> m_helicopter;  /* repli procédural */
    std::unique_ptr<render::LoadedHelicopter> m_loadedHeli; /* modèle FlightGear si présent */

    /* --- Entrées, physique et pilotage automatique ---------------------------------- */

    std::unique_ptr<input::InputSystem> m_input;
    physics::FlightModel m_flight;
    physics::FlightAssist m_assist; /* mode assisté : confort de pilotage (touche M / LB) */
    DemoPilot m_demo;               /* pilote automatique du mode démo (inactif par défaut) */
    LandingAutopilot m_autoland;    /* atterrissage automatique (touche J / RB) */

    /* --- Audio et radio --------------------------------------------------------------- */

    std::filesystem::path m_musicPath; /* musique jouée pendant la démo (assets/music/demo.mp3) */
    std::string m_radioUrl; /* URL du flux radio résolue au démarrage (touche K l'allume/coupe) */

    /* --- Cycle jour/nuit --------------------------------------------------------------- */

    float m_sunTimeScale = 1.0f; /* vitesse du temps : 1 = réel, 144 = jour en 10 min, 0 = figé */
    /* Multiplicateur appliqué à la vitesse ci-dessus entre le coucher et le lever
       (clé lune_vitesse, 2 par défaut) : la nuit passe donc deux fois plus vite que
       le jour. Voir timeOfDaySeconds (ApplicationSun.cpp). */
    float m_nightSpeedFactor = 2.0f;
    float m_sunBaseSeconds = 0.0f; /* heure locale du PC au lancement (s depuis minuit) */
    bool m_demoWasActive = false;  /* pour couper la musique quand la démo s'arrête */
    bool m_demoUserView =
        false; /* en démo : l'utilisateur a repris la main sur la vue (touche C) */
    bool m_demoUserHud = false; /* en démo : l'utilisateur a repris la main sur le HUD (touche H) */

    /* --- HUD et vues -------------------------------------------------------------------- */

    ui::Hud m_hud;
    audio::AudioEngine m_audio;
    /* 0 poursuite, 1 cockpit, 2 orbite, 3 orbite solaire. Le vol commence en
       cockpit : c'est la place du pilote, et le tableau de bord est le premier
       repère dont il a besoin. La touche C fait le tour des quatre vues. */
    int m_viewMode = 1;
    /* Origine de rendu (rendu relatif à la caméra) : on retranche cette position
       horizontale (X, Z ; Y laissé à 0 pour préserver les altitudes) de la caméra et
       de toutes les géométries avant de les confier au GPU. Les coordonnées près de la
       caméra restent ainsi petites, ce qui supprime le tremblement de précision float32
       en grandes coordonnées monde (terrain de plusieurs dizaines de km). Posée au
       début de renderScene, lue par les fonctions de rendu sol/effets. */
    vec3 m_renderOrigin{0.0f};
    /* Changement de vue : cut net (voir Camera::cut). On garde la vue précédente pour
       détecter le changement. */
    int m_prevCamView = -1;    /* vue précédente (caméra), -1 au départ */
    float m_orbitStart = 0.0f; /* instant d'entrée en vue orbite (démo : un tour complet) */
    /* Temps d'animation : avance comme le temps réel, mais se fige en pause (et
       pendant les panneaux de confirmation). Pilote la démo, la caméra d'orbite et
       la vibration du cockpit pour qu'ils s'arrêtent vraiment en pause. */
    float m_animTime = 0.0f;
    /* Cadence lissée (images/s) pour l'affichage du HUD 4 coins : moyenne mobile
       exponentielle du frameDt, pour un chiffre stable et lisible. */
    float m_fpsSmoothed = 0.0f;
    bool m_nrLedArmed = false; /* LED NR : rotor arrivé en régime (voir fillHud) */

    /* --- Menu et configuration ------------------------------------------------------------ */

    /* Choix du menu de démarrage (voir runStartupMenu), prioritaires sur config.txt
       mais pas sur les variables d'environnement. Terrain vide = pas de choix menu ;
       turbine -1 = pas de choix, 0 = à froid, 1 = démarrée. */
    std::string m_menuTerrain;
    int m_menuTurbine = -1;
    bool m_menuDemo = false;   /* le menu a lancé la démo (bouton "Démo") */
    bool m_menuCombat = false; /* le menu a lancé le mode zombie (bouton "Mode Zombie") */

    /* Vrai quand la démo en cours a été lancée depuis le menu de démarrage : en sortir
       (Échap, reprise des commandes...) ramène alors au menu, au lieu de rendre la main
       en vol libre. La démo lancée par config/env (ARTOUSTE_DEMO) garde l'ancien
       comportement (arrêt de la démo, vol libre). */
    bool m_demoFromMenu = false;

    /* Délai de grâce (s) après le (re)lancement d'une démo, pendant lequel une entrée
       pilote ne l'annule pas. Sans lui, la touche D qui lance la démo depuis le menu
       est aussi le palonnier droit en vol (Keyboard.cpp) : si le relâchement physique
       de la touche accuse ne serait-ce que quelques dizaines de ms de retard sur le
       premier appel de computeControls, ce résidu dépasse aussitôt le seuil de 0.15 et
       coupe la démo -- retour au menu immédiat, alors que le pilote n'a rien demandé. */
    float m_demoInputGraceS = 0.0f;

    /* Configuration lue au lancement (assets/config.txt). Chargée tôt dans run(),
       avant l'ouverture de la fenêtre, car le MSAA doit être connu à sa création ;
       réutilisée ensuite par initScene (terrain, démo, végétation...). */
    app::Config m_config;

    /* Recherche d'une version plus récente (clé "verifier_maj" de config.txt,
       coupée par ARTOUSTE_NO_MAJ). Lancée dans run() avant l'ouverture de la
       fenêtre, consultée par le menu de démarrage, qui propose alors d'ouvrir la
       page du projet. */
    app::MiseAJour m_maj;

    /* Végétation active (clé "arbres" de config.txt, vrai par défaut, forcée à faux
       par ARTOUSTE_NO_TREES). Lue par loadTerrain pour semer ou non les arbres. */
    bool m_treesEnabled = true;

    /* Budget d'arbres effectif (clé "arbres_max" de config.txt, surchargée par
       ARTOUSTE_TREE_MAX), résolu dans initScene et passé à Vegetation par loadTerrain.
       0 = laisser Vegetation appliquer son défaut. */
    std::size_t m_treeBudget = 0;

    /* Côté de la fenêtre de tuiles fines (clé "tuiles_fenetre_px" de config.txt,
       surchargée par ARTOUSTE_TUILES_FENETRE), résolu dans initScene et passé à
       Terrain par loadTerrain. 0 = pas de détail fin. */
    int m_detailWindowPx = 0;

    /* Budget de sommets du relief dessiné (clé "relief_sommets_max", surchargée
       par ARTOUSTE_RELIEF_SOMMETS). 0 = dessiner tous les points de la carte
       d'altitude. */
    int m_reliefVertexBudget = 0;

    /* Options en vigueur pour le terrain actuellement chargé. Le gestionnaire de
       cartes peut les changer pendant que la scène est en mémoire : on les garde
       pour savoir, au retour du menu, s'il faut recharger la carte (voir
       applyMenuSession). */
    OptionsCarte m_optionsChargees;

    /* Le gestionnaire de cartes a fabriqué ou supprimé des tuiles pendant que la
       scène était en mémoire. Ni la carte choisie ni son options.txt n'en portent
       la trace, et pourtant la fenêtre de détail se construit au chargement du
       terrain : sans ce drapeau, des tuiles fraîchement téléchargées ne
       s'affichaient qu'au redémarrage du jeu. */
    bool m_cartesRemaniees = false;

    /* --- État de session (menu, pause, plein écran) ---------------------------------------- */

    /* Passe à true quand l'utilisateur appuie sur Échap en vol : la boucle de vol rend
       la main pour réafficher le menu de démarrage (au lieu de quitter). */
    bool m_returnToMenu = false;

    /* Vrai pendant l'affichage du menu de démarrage : le callback clavier de vol
       (keyCallback) s'efface alors, car le menu lit ses propres entrées et la scène
       peut ne pas être initialisée (le menu s'affiche avant initScene au lancement). */
    bool m_inMenu = false;

    /* Plein écran sans bordure (voir setFullscreen). Géométrie de la fenêtre mémorisée
       avant de passer en plein écran, pour la restituer au retour en fenêtré (touche F). */
    bool m_fullscreen = false;
    int m_winX = 0;
    int m_winY = 0;
    int m_winW = 0;
    int m_winH = 0;
    /* Dernières commandes calculées : réutilisées en pause pour que les gouvernes
       dessinées et le HUD gardent leur position au lieu de revenir au neutre. */
    physics::Controls m_lastControls{};
    /* Message radio (voix de synthèse + sous-titre). */
    bool m_radioMsgArmed = false; /* compte à rebours en cours */
    bool m_radioMsgDone = false;  /* déjà émis depuis le dernier démarrage turbine */
    float m_radioMsgDelay = 0.0f; /* s avant émission, une fois armé */
    float m_radioMsgShow = 0.0f;  /* s restantes d'affichage du sous-titre */
    std::string m_radioMsg;       /* texte du message courant (anglais) */
    std::string m_homeStation;    /* nom de l'hélipad de départ (-> tour de contrôle) */
    /* Message court de l'atterrissage automatique (échec de l'engagement ou
       auto-désengagement), affiché quelques secondes dans le HUD. */
    std::string m_autolandMsg;
    float m_autolandMsgShow = 0.0f;
    ui::HudMode m_hudMode = ui::HudMode::Overlay; /* HUD complet au lancement ; H fait défiler coins
                                                     -> superposé -> rien */
    bool m_paused = false;
    bool m_confirmReset = false; /* panneau Oui/Non avant un reset (touche X/R) */
    bool m_confirmDemo = false;  /* panneau Oui/Non avant de lancer la démo (réservé) */

    /* --- Livrée et rotor ------------------------------------------------------------------- */

    render::Livery m_livery =
        render::Livery::Gendarmerie; /* livrée par défaut (touche L / bouton A) */
    float m_rotorAngle =
        0.0f; /* angle du rotor principal (rad) : rotation au régime rotor, parking à l'arrêt */
    float m_parkOffset =
        0.0f; /* décalage aléatoire de la position de parking (pale pas pile dans l'axe) */
    float m_closingSpeed =
        0.0f; /* vitesse de rapprochement caméra<->appareil lissée (effet Doppler, vue orbite) */
    physics::Turbine::State m_prevTurbineState =
        physics::Turbine::State::Arret; /* pour déclencher le son de démarrage au bon moment */

    /* --- Aide à l'atterrissage --------------------------------------------------------------
     * État persistant entre les images (mode assisté).
     * ------------------------------------------------------------------------------------- */
    float m_scoreTimer = 0.0f;  /* temps restant d'affichage du score (s) */
    float m_lastScoreM = -1.0f; /* dernière distance au posé (m), -1 si aucun */
    bool m_wasOnGround = false; /* état sol de l'image précédente (anti-rebond) */
    bool m_wasAirborne = false; /* a volé depuis l'activation : évite un faux score au sol */
    bool m_hasFlown = false; /* a décollé depuis le lancement/reset : pas d'aide au posé tant qu'on
                                n'a pas volé */
    float m_padGuideGrace = 0.0f; /* s restantes sans réticule après un décollage du pad */
};

} /* namespace artouste::app */
