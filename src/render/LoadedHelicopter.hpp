/*
 * LoadedHelicopter.hpp
 * Hélicoptère assemblé à partir des fichiers du modèle FlightGear (.ac) :
 * fuselage texturé, intérieur, planche de bord et rotors (moyeu et pales
 * chargés séparément, puis animés à régime fixe).
 *
 * Les fichiers FlightGear utilisent un repère différent du nôtre. Au chargement
 * et au dessin, on applique donc des corrections (rotation de 180 degrés et
 * décalage vertical pour poser les patins au sol) afin d'orienter et de placer
 * l'appareil correctement dans la scène.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Model.hpp"
#include "util/Math.hpp"

#include <filesystem>
#include <vector>

namespace artouste::render {

class Shader;

class LoadedHelicopter {
public:
    explicit LoadedHelicopter(const std::filesystem::path& modelsDir);

    /* Vrai si le fuselage a bien été chargé. Sinon, l'appelant se rabat sur
       l'hélicoptère de remplacement construit par le code. */
    [[nodiscard]] bool loaded() const noexcept { return !m_fuselage.empty(); }

    /* Dessine l'appareil complet. 'base' place et oriente l'hélicoptère dans le
       monde ; 'rotorAngle' est l'angle du rotor principal (rad), géré par
       l'application (rotation au régime du rotor, puis parking à l'arrêt).
       'fullPilot' vaut vrai en vues externes (pilote entier) et faux en vue
       cockpit (pilote sans tête ni casque : on voit ses bras et ses jambes, mais
       la caméra n'est plus enfermée dans sa tête).
       'rudder' est la commande de palonnier ([-1, +1]) : elle fait basculer les
       pédales et, en vue cockpit, les jambes du pilote.
       'cyclicLong' et 'cyclicLat' ([-1, +1]) inclinent le manche cyclique (tangage
       et roulis) ; en vue cockpit, l'avant-bras droit du pilote suit le manche.
       'collective' ([0, +1]) lève le levier de collectif (commande de la main gauche). */
    void draw(Shader& shader, const mat4& base, float rotorAngle, bool fullPilot = true,
              float rudder = 0.0f, float cyclicLong = 0.0f, float cyclicLat = 0.0f,
              float collective = 0.0f, float rollRad = 0.0f, float pitchRad = 0.0f,
              float altitudeFt = 0.0f, float varioFpm = 0.0f, float headingRad = 0.0f,
              float airspeedKt = 0.0f, float torquePct = 0.0f) const;

    /* Centre du disque rotor dans le monde (pour dessiner un jour le disque flou
       translucide à haut régime, voir le code mis en commentaire). 'base' est la
       pose de l'appareil. */
    [[nodiscard]] vec3 mainHubWorld(const mat4& base) const;

    /* Active (ou non) la livrée Gendarmerie nationale sur le fuselage. Par défaut
       l'appareil garde sa livrée d'origine. */
    void setGendarmerieLivery(bool on);

    /* Rayon du rotor principal, en mètres (longueur d'une pale). */
    static constexpr float MAIN_ROTOR_RADIUS = 5.0f;

    /* Distance horizontale, vers l'avant, entre l'origine du modèle (que la
       physique place dans le monde) et l'axe du mât rotor. Vaut la composante X du
       moyeu MAIN_HUB une fois la correction de repère appliquée. Sert à garer
       l'appareil mât centré sur le H, et non son origine. */
    static constexpr float ROTOR_FORWARD_OFFSET = 2.142f;

private:
    /* Un cadran de la planche de bord avec sa position relative au panneau. */
    struct Gauge {
        Model model;
        vec3  offset;
    };

    /* Petit raccourci de rendu : fixe la matrice du modèle puis dessine la pièce. */
    void drawModel(Shader& shader, const Model& model, const mat4& transform, Pass pass) const;

    /* Chargement des six instruments animés du tableau de bord (horizon, altimètre,
       vario, compas, anémomètre, couplemètre), chacun découpé en cadran fixe et
       parties mobiles. Appelé par le constructeur. */
    void loadInstruments(const std::filesystem::path& dir);

    /* Dessine la cellule : fuselage, intérieur, pilotes, et les commandes animées
       (jambes, bras du pilote, palonnier, manche cyclique, levier de collectif). */
    void drawAirframe(Shader& shader, const mat4& root, bool fullPilot, float rudder,
                      float cyclicLong, float cyclicLat, float collective) const;

    /* Dessine la planche de bord et les six instruments animés selon l'état de vol. */
    void drawInstruments(Shader& shader, const mat4& root, float rollRad, float pitchRad,
                         float altitudeFt, float varioFpm, float headingRad, float airspeedKt,
                         float torquePct) const;

    /* Dessine les rotors (moyeux et pales, principaux et de queue), animés à partir
       de l'angle du rotor principal. */
    void drawRotors(Shader& shader, const mat4& root, float rotorAngle) const;

    /* Passe transparente : marquages de la livrée Gendarmerie puis vitrages. */
    void drawLivery(Shader& shader, const mat4& root) const;

    Model              m_fuselage;
    const Texture*     m_liveryGendarmerie = nullptr;  /* livrée bleue (fuselage) */
    bool               m_gendarmerie = false;          /* livrée Gendarmerie active (marquages) */
    Model              m_interior;
    Model              m_pilot;         /* pilote entier (vues externes) */
    Model              m_pilotCockpit;  /* pilote sans tête, casque ni jambes (vue cockpit) */
    Model              m_legLeft;       /* jambe gauche du pilote (animée au palonnier) */
    Model              m_legRight;      /* jambe droite du pilote (animée au palonnier) */
    Model              m_armUpper;      /* haut du bras droit, fixe (au repos) */
    Model              m_forearm;       /* avant-bras droit (plie au coude) */
    Model              m_grip;          /* poignée (manche) : suit rigidement le manche */
    vec3               m_elbowLocal{0.0f};  /* jonction réelle haut du bras / avant-bras */
    vec3               m_wristLocal{0.0f};  /* jonction réelle avant-bras / poignée */
    Model              m_armUpperLeft;  /* haut du bras gauche, fixe (au repos) */
    Model              m_forearmLeft;   /* avant-bras gauche, avec la main (plie au coude vers le collectif) */
    vec3               m_elbowLeftLocal{0.0f};  /* jonction réelle haut du bras gauche / avant-bras */
    vec3               m_handLeftLocal{0.0f};   /* bout de l'avant-bras gauche (la main, au repos) */
    vec3               m_collectiveGripLocal{0.0f};  /* poignée au bout du levier (repère du levier) */
    Model              m_pedalLeft;     /* pédale gauche (paloG, animée au palonnier) */
    Model              m_pedalRight;    /* pédale droite (paloD, animée au palonnier) */
    Model              m_cyclic;        /* manche cyclique (recopié devant chaque siège) */
    Model              m_collectiveBase;  /* embase du levier de collectif (fixe) */
    Model              m_collectiveLever; /* levier de collectif (pivote avec la commande) */
    Model              m_panel;
    std::vector<Gauge> m_gauges;

    /* Horizon artificiel (indicateur d'assiette) animé : on charge le cadran ai.ac
       en trois morceaux, pour reproduire l'animation FlightGear (ai.xml). Le statique
       (lunette, repères, symbole avion) ne bouge pas ; la carte (background, scale)
       tourne avec le roulis ; la barre d'horizon (float) tourne avec le roulis ET se
       translate avec le tangage. */
    Model              m_aiStatic;
    Model              m_aiCard;
    Model              m_aiFloat;
    vec3               m_aiOffset{0.0f};
    bool               m_hasAi = false;

    /* Altimètre animé : cadran statique + trois aiguilles (centaines, milliers et
       dizaines de milliers de pieds) qui tournent autour de X avec l'altitude. */
    Model              m_altStatic;
    Model              m_altN100;
    Model              m_altN1000;
    Model              m_altN10000;
    vec3               m_altOffset{0.0f};
    bool               m_hasAlt = false;

    /* Vario (VSI) animé : cadran statique + aiguille unique qui tourne autour de X
       selon la vitesse verticale (table non linéaire relevée sur la face du cadran). */
    Model              m_vsiStatic;
    Model              m_vsiNeedle;
    vec3               m_vsiOffset{0.0f};
    bool               m_hasVsi = false;

    /* Compas (conservateur de cap) animé : cadran fixe (symbole avion, index) + rose
       des vents mobile qui tourne autour de X avec le cap. */
    Model              m_hiStatic;
    Model              m_hiCard;
    vec3               m_hiOffset{0.0f};
    bool               m_hasHi = false;

    /* Anémomètre (ASI) animé : cadran statique + aiguille unique qui tourne autour de X
       proportionnellement à la vitesse air (face linéaire, ~1.75 deg/kt). */
    Model              m_asiStatic;
    Model              m_asiNeedle;
    vec3               m_asiOffset{0.0f};
    bool               m_hasAsi = false;

    /* Couplemètre (torque) animé : cadran statique + aiguille unique qui tourne autour
       de X selon le couple estimé en pourcentage (face linéaire, ~2.73 deg/%). */
    Model              m_torqueStatic;
    Model              m_torqueNeedle;
    vec3               m_torqueOffset{0.0f};
    bool               m_hasTorque = false;
    Model              m_mainHub;
    Model              m_mainBlade;
    Model              m_tailHub;
    Model              m_tailBlade;
    Model              m_decalGendarmerie;  /* mot "GENDARMERIE" (flancs de cabine) */
    Model              m_decalReg;          /* immatriculation "F-BRHP" */
    Model              m_decalStripe;       /* liséré blanc (bas de cabine) */
};

}  /* namespace artouste::render */
