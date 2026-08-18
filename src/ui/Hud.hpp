/*
 * Hud.hpp
 * HUD transparent : affiche les informations de vol en surimpression,
 * par-dessus la scène 3D, grâce à Dear ImGui. La touche H fait défiler les modes
 * d'affichage (quatre coins, instruments superposés, rien). L'ensemble est mis à
 * l'échelle de la fenêtre (référence 1280x720) via updateScale, menu compris.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include <imgui.h>

#include <string>
#include <vector>

struct GLFWwindow;

namespace artouste::ui {

/* Lieu remarquable à signaler : étiquette posée sur la scène 3D (si visible) et
   point sur la minimap. */
struct HudLabel {
    std::string name; /* texte de l'étiquette (peut inclure altitude + distance) */
    float fx = 0.0f;  /* position écran de l'étiquette (fraction 0-1) */
    float fy = 0.0f;
    float depth = 1e30f;   /* profondeur caméra (plus petit = plus proche), pour trier */
    bool generic = false;  /* étiquette générique ("Hélisurface") : son nom cède la
                              place à celui d'un lieu nommé en cas de chevauchement */
    bool onScreen = false; /* le lieu est devant la caméra et dans le cadre */
    float mapU = 0.0f;     /* position sur la minimap : 0 ouest -> 1 est */
    float mapV = 0.0f;     /* 0 nord -> 1 sud */

    /* Pad équipé d'une balise HAPI (voir render::Terrain::hapiUnitNear) : son point
       (scène 3D et minimap) adopte la couleur et le clignotement de la balise
       plutôt que la couleur générique, pour repérer la pente d'approche sans
       chercher la lueur au sol. */
    bool hasHapi = false;
    bool hapiGreen = true; /* secteur courant : vert (sur/au-dessus la pente) ou rouge */
    bool hapiOff = false;  /* clignotement en phase éteinte (secteurs trop haut/trop bas) */
};

/* Mode d'affichage du HUD, parcouru en boucle par la touche H / le bouton B. */
enum class HudMode {
    Corners, /* quatre panneaux texte dans les coins */
    Overlay, /* instruments ronds verts superposés (Super HUD) */
    Off      /* aucun affichage */
};

/* Valeurs à afficher, déjà converties dans les unités du HUD. */
struct HudData {
    float altitudeM = 0.0f;
    float airspeedKmh = 0.0f; /* vitesse air en km/h (unité d'époque, Alouette II FR) */
    float headingDeg = 0.0f;
    float varioMs = 0.0f; /* taux de montée en m/s (HUD coins et Super HUD) */
    float collectivePct = 0.0f;
    float pasDeg        = 0.0f;     /* pas collectif réel en degrés de pale (12-15 en vol) */
    float rotorPct = 0.0f;
    float rotorRpm = 0.0f;          /* régime rotor en tr/min */
    bool rotorLedArmed = false;     /* LED du cadran NR active : le rotor a atteint son
                                       régime depuis le dernier lancement de la turbine
                                       (évite le rouge pendant démarrage et extinction) */
    bool rotorSpoolingUp = false;   /* rotor en cours de montée en régime (embrayage,
                                     juste après le lâcher du frein) : fait clignoter
                                     la LED NR tant que le régime nominal n'est pas
                                     atteint, plutôt que de la laisser éteinte */
    float turbineRpm = 0.0f;        /* régime turbine en tr/min */
    bool turbineSpoolingUp = false; /* turbine en cours de montée en régime
                                   (démarrage) : fait clignoter la LED TURBINE
                                   tant que le régime nominal n'est pas atteint,
                                   plutôt que de la laisser éteinte */
    float exhaustTempC = 0.0f;      /* température tuyère (T4) en degrés Celsius */
    float fuelLiters = 0.0f;        /* carburant restant, en litres */
    const char* turbine = "";       /* libellé d'état de la turbine */
    bool assist = false;            /* mode assisté actif : affiche un repère */
    bool autoland = false;          /* atterrissage automatique engagé : affiche un repère */
    float hvIntensity  = 0.0f;      /* zone à éviter hauteur-vitesse, 0..1 lissée :
                                       au-delà de 0,5, indicateur discret (voir
                                       renderHvAlert) */
    float vrsIntensity = 0.0f;      /* vortex ring state, 0..1 : au-delà d'un seuil,
                                       un bandeau d'alerte clignotant s'affiche (l'appareil
                                       d'époque n'a pas d'avertisseur : c'est une aide du
                                       simulateur, hors cockpit) */
    bool sinkRateAlert = false;     /* taux de descente élevé près du sol (façon GPWS) :
                                       bandeau "TAUX DE DESCENTE" clignotant et LED rouge
                                       du cadran V/S. Aide du simulateur, calculée dans
                                       fillHud (descente rapide + AGL faible) */
    bool radio = false;             /* flux radio en lecture : affiche un voyant "RADIO" */
    bool reliefLiserets = false;    /* liserets de la fenêtre de relief tracés au sol
                                       (clé relief_debug ET fenêtre chargée) : affiche
                                       leur légende, sans quoi les traits colorés ne
                                       s'expliquent pas */
    int radioMixPct = 0;       /* part de la radio dans le crossfade radio/hélico (0 à 100 %) */
    bool geoValid = false;     /* coordonnées géographiques disponibles */
    float lonDeg = 0.0f;       /* longitude (degrés, + est) */
    float latDeg = 0.0f;       /* latitude (degrés, + nord) */
    float timeOfDaySec = 0.0f; /* heure du simulateur (s depuis minuit) */
    float timeScale = 1.0f;    /* vitesse du temps (1 = temps réel) */
    bool colonOn = true;       /* deux-points de l'horloge HH:MM (clignote 1 Hz) */
    bool alarmBlinkOn = true;  /* phase de clignotement des LED d'alarme jaune/rouge
                                  (~2 Hz) : à false, la LED s'éteint le temps du
                                  battement, pour attirer l'oeil. true = LED allumée
                                  (et en capture, LED figée allumée) */
    float fps = 0.0f;          /* images par seconde (lissées) ; affiché dans le coin
                                  bas-droit du HUD 4 coins. 0 = masqué (ex. en capture) */

    /* Aide à l'atterrissage : hélipad le plus proche en finale (active seulement
       en mode assisté, voir ApplicationHud.cpp). */
    struct PadGuidance {
        bool active = false;      /* true seulement si conditions remplies */
        float dx = 0.0f;          /* écart latéral en mètres (+ = droite pilote) */
        float dz = 0.0f;          /* écart longitudinal en mètres (+ = devant) */
        float distanceM = 0.0f;   /* distance 2D au centre du pad */
        float altAbovePad = 0.0f; /* altitude au-dessus du pad (pas du sol général) */
        const char* name = "";    /* nom du pad (pour l'étiquette HUD) */

        /* Score du dernier posé. */
        float scoreM = -1.0f; /* distance au centre au moment du posé, en mètres */
        bool scored = false;  /* true pendant SCORE_DISPLAY_S secondes après le posé */
    };
    PadGuidance padGuidance;

    /* Repérage : lieux remarquables et minimap. */
    std::vector<HudLabel> labels; /* lieux à étiqueter (scène 3D + carte) */
    unsigned int mapTexId = 0;    /* orthophoto pour la minimap (0 = pas de carte) */
    float mapHeliU = 0.0f;        /* position de l'appareil sur la carte (0-1) */
    float mapHeliV = 0.0f;
    float mapHeadingDeg = 0.0f; /* cap, pour orienter le marqueur */

    /* Message radio reçu : sous-titre centré en bas, affiché tant que la chaîne n'est
       pas vide. Le pointeur appartient à l'appelant (durée de vie gérée côté app). */
    const char* radioMessage = "";

    /* Message court de l'atterrissage automatique (échec de l'engagement, faute de
       pad à portée, ou auto-désengagement) : bandeau temporaire, même mécanisme que
       radioMessage. */
    const char* autolandMessage = "";

    /* Mode zombie : vie, munitions, vague, chrono et fin de partie (voir
       app::CombatMode). belowCeiling rend visible la mécanique du plafond
       d'altitude des boulettes toxiques (TOXIC_CEILING_M) : sans ce repère, le
       joueur ne peut pas deviner pourquoi il se fait toucher ou non. */
    struct CombatHud {
        bool active = false;
        float healthPct = 1.0f;
        int ammoCurrent = 0;
        int ammoMax = 0;
        int wave = 0;
        float elapsedS = 0.0f;
        bool belowCeiling = false; /* sous le plafond : les zombies peuvent viser */
        bool gameOver = false;
        int score = 0; /* points, voir app::CombatMode::score */
        int kills = 0; /* zombies tués depuis le début de la session */

        /* Annonce à afficher (0 = aucune, 1 = double, 2 = triple, 3 = carnage,
           4 = largueur neutralisé), voir app::CombatMode::KillAnnouncement -- valeur
           brute plutôt que l'enum pour garder HudData indépendant du module
           combat, comme le reste de cette structure (vec3 exclu, voir
           zombieMapPoints plus bas). */
        int killAnnounceKind = 0;

        /* Largueur (boss d'une manche sur cinq, voir app::WaveManager) : jauge de
           vie affichée seulement tant qu'il est debout. */
        bool broodActive = false;
        float broodHealthPct = 1.0f;

        /* Points des zombies sur la minimap (fractions 0-1 dans l'emprise du
           terrain, même convention que HudLabel::mapU/mapV). Pas de type vec2
           ici : Hud.hpp reste volontairement indépendant de glm/util::Math,
           comme le reste de HudData (lonDeg/latDeg en float bruts, etc.). */
        struct MapPoint {
            float u = 0.0f;
            float v = 0.0f;
        };
        std::vector<MapPoint> zombieMapPoints;

        /* Mire du canon fixe : projection écran (fraction 0-1) d'un point loin
           devant l'appareil dans l'axe de tir (voir Application::fillHud). Le
           canon ne suit pas le regard (pas de viseur mobile, voir Weapon) --
           sans repère, impossible de savoir où partent réellement les balles.
           reticleOnScreen faux si ce point tombe hors du cadre (canon qui ne
           vise pas dans la direction actuellement regardée par la caméra). */
        bool reticleOnScreen = false;
        float reticleFx = 0.5f;
        float reticleFy = 0.5f;
    };
    CombatHud combat;
};

class Hud {
public:
    /* Cycle de vie du contexte ImGui. Définies dans HudSetup.cpp. */
    void init(GLFWwindow* window);
    void shutdown();

    /* Construit et dessine la surimpression d'une image selon le mode choisi,
     * plus le bandeau de pause si paused (le tout en une seule frame ImGui). */
    /* confirmDemo : affiche le panneau de confirmation avant de lancer la démo.
       forceLabels : afficher les étiquettes des lieux même quand le HUD est éteint
       (mode Off), sans les instruments ni la minimap. Utile pour la démo. */
    void render(const HudData& data,
                HudMode mode,
                bool paused,
                bool confirmReset = false,
                bool confirmDemo = false,
                bool forceLabels = false);

    [[nodiscard]] bool ready() const noexcept { return m_ready; }

    /* Met à jour le facteur d'échelle du HUD à partir de la taille du framebuffer et
       reconstruit la police à la bonne taille si besoin. À appeler chaque image, AVANT
       le NewFrame ImGui (la reconstruction d'atlas en dépend) : la boucle de vol, la
       capture et le menu de démarrage le font avec la taille qu'ils ont déjà sous la
       main, render() n'interroge donc pas GLFW. Définie dans HudSetup.cpp. */
    void updateScale(int framebufferWidth, int framebufferHeight);

private:
    /* Sous-affichages d'une image, appelés par render() selon le mode. 'w'/'h' sont
       les dimensions de l'écran, 'm' la marge depuis les bords. Définie dans
       HudCorners.cpp. */
    void renderCorners(const HudData& data, float w, float h, float m);
    void renderOverlay(const HudData& data, float w, float h, float m);
    /* Aide à l'atterrissage (réticule + score), dessinée par-dessus tous les modes
       de HUD, y compris quand il est éteint en démo. */
    void renderPadGuidance(const HudData& data, float w, float h);
    void renderLabels(const HudData& data, float w, float h);
    void renderMinimap(const HudData& data, HudMode mode, float m);
    void renderBanners(bool paused, bool confirmReset, bool confirmDemo, float w, float h);
    /* Alerte vortex ring state : bandeau rouge clignotant, par-dessus tous les modes.
       Ne s'affiche que si l'intensité VRS dépasse le seuil (l'appareil réel n'a aucun
       avertisseur : c'est une aide du simulateur). Définie dans HudAlerts.cpp. */
    void renderVortexAlert(const HudData& data, float w, float h);
    /* Alerte taux de descente (GPWS) : bandeau rouge clignotant quand la descente est
       trop rapide pres du sol. Aide du simulateur, comme l'alerte vortex. Définie
       dans HudAlerts.cpp. */
    void renderSinkRateAlert(const HudData& data, float w, float h);

    /* Bandeau rouge clignotant quand la VNE est franchie (LED IAS au rouge). Le
       préavis jaune, lui, reste au seul cadran. Défini dans HudAlerts.cpp. */
    void renderVitesseAlert(const HudData& data, float w, float h);
    /* Indicateur discret de la zone à éviter du diagramme hauteur-vitesse
       (autorotation non garantie en cas de panne) : texte ambre fixe, sans
       clignotement, l'appareil réel n'ayant aucun avertisseur. Défini dans
       HudAlerts.cpp. */
    void renderHvAlert(const HudData& data, float w, float h);
    /* Sous-titre d'un message radio reçu, centré en bas, par-dessus tous les modes. */
    void renderRadioSubtitle(const HudData& data, float w);
    /* Bandeau du message de l'atterrissage automatique, par-dessus tous les modes. */
    void renderAutolandMessage(const HudData& data, float w);
    /* Mode zombie : panneau vie/munitions/vague/chrono/plafond, par-dessus tous les
       modes (comme les alertes de vol) ; bandeau de fin de partie si gameOver.
       Ne dessine rien si data.combat.active est faux. */
    void renderCombatHud(const HudData& data, HudMode mode, float w, float h);

    bool m_ready = false;
    float m_builtFontScale = 0.0f; /* échelle de la police déjà construite (0 = jamais) */
    ImGuiStyle m_baseStyle;        /* style non mis à l'échelle, base de ScaleAllSizes */
};

} /* namespace artouste::ui */
