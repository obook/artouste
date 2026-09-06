/*
 * FlightModel.hpp
 * Vrai modèle de vol : il rassemble toutes les forces et tous les couples qui
 * agissent sur l'hélicoptère (poussée du rotor, gravité, traînée, commandes,
 * anti-couple, amortissements), puis les confie au corps rigide pour avancer.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/Controls.hpp"
#include "physics/RigidBody.hpp"
#include "physics/Turbine.hpp"
#include "physics/constants.hpp"

#include <algorithm>

namespace artouste::physics {

/* Suite donnée à une demande de bascule de la turbine (touche T, bouton Start) :
   faite, ou refusée avec son motif, pour que l'appelant puisse le dire au pilote
   plutôt que de laisser la commande sans effet visible. */
enum class ActionTurbine { Faite, ReservoirTropBas, PasAuSol };

class FlightModel {
public:
    /* Avance la simulation d'un pas de durée dt (appelé à cadence régulière). */
    void update(const Controls& controls, float dt) noexcept;

    /* Turbine Artouste : on l'expose pour la piloter (démarrage/arrêt) et lire
     * son état (HUD, audio). Le modèle multiplie poussée et anti-couple par son
     * régime ; il faut donc la démarrer pour décoller. Repositionner l'appareil
     * (reset) ne touche pas à la turbine : elle garde son état. */
    [[nodiscard]] Turbine&       turbine() noexcept { return m_turbine; }
    [[nodiscard]] const Turbine& turbine() const noexcept { return m_turbine; }

    /* Repositionner l'appareil refait le plein (retour à la base) ; la turbine,
       elle, garde son état. */
    void reset() noexcept {
        m_body      = RigidBody{};
        m_fuelLiters = FUEL_CAPACITY_L;
        clearGroundImpact();
        clearRotorLag();
    }

    /* Réinitialise à une altitude donnée, pratique pour tester loin du sol. */
    void reset(float altitude) noexcept {
        m_body            = RigidBody{};
        m_body.position.y = altitude;
        m_fuelLiters      = FUEL_CAPACITY_L;
        clearGroundImpact();
        clearRotorLag();
    }

    /* Réinitialise à une position donnée (par exemple posé sur la côte). */
    void reset(const vec3& position) noexcept {
        m_body          = RigidBody{};
        m_body.position = position;
        m_fuelLiters    = FUEL_CAPACITY_L;
        clearGroundImpact();
        clearRotorLag();
    }

    /* Réinitialise à une position et un cap donnés (degrés boussole : 0 = nord,
       90 = est, 180 = sud). L'axe avant du corps est +X, soit l'est à l'identité ;
       on tourne donc autour de la verticale de (90 - cap) degrés. */
    void reset(const vec3& position, float headingDeg) noexcept {
        reset(position);
        m_body.orientation = glm::angleAxis(glm::radians(90.0f - headingDeg),
                                            vec3{0.0f, 1.0f, 0.0f});
    }

    /* Altitude du sol (m) sous l'appareil : le contact se fait à cette hauteur
       plutôt qu'au niveau de la mer, pour suivre le relief du terrain. */
    void setGroundHeight(float h) noexcept { m_groundHeight = h; }

    /* Active ou non les difficultés de pilotage (perte de puissance en altitude,
       traînée d'onde au-delà de la VNE, perte d'autorité au palonnier en vol
       latéral, vortex ring state). On les coupe en mode assisté et en démo, pour
       garder un vol facile et prévisible. Les effets qui aident le pilote (effet de
       sol, effet de translation) restent toujours actifs. */
    void setRealFlyPhysicsEnabled(bool enabled) noexcept { m_realFlyPhysicsEnabled = enabled; }

    [[nodiscard]] const RigidBody& body() const noexcept { return m_body; }

    /* Dernière poussée calculée, en newtons : utile pour le débogage et l'affichage. */
    [[nodiscard]] float lastThrust() const noexcept { return m_lastThrust; }

    /* Carburant restant, en litres (pour le HUD et le voyant d'alerte). */
    [[nodiscard]] float fuelLiters() const noexcept { return m_fuelLiters; }

    /* Démarre ou coupe la turbine, comme Turbine::toggle, mais en tenant compte du
       réservoir : un démarrage est REFUSÉ sous FUEL_START_MIN_L, une coupure est
       toujours acceptée. Rend vrai si l'état a changé.

       Sans ce garde-fou, appuyer sur le démarreur à sec lançait la séquence et son
       vacarme d'une minute, pour une extinction juste avant le régime de vol. Le
       cas le plus traître n'est même pas le réservoir vide (coupé au premier pas de
       simulation) mais le fond de réservoir : la jauge affiche des litres entiers,
       donc "0 L" peut cacher un demi-litre, de quoi amorcer un démarrage
       parfaitement inutile. */
    [[nodiscard]] ActionTurbine toggleTurbine() noexcept {
        const bool aLArret = m_turbine.state() == Turbine::State::Arret ||
                             m_turbine.state() == Turbine::State::Extinction;
        if (aLArret) {
            if (m_fuelLiters < FUEL_START_MIN_L) {
                return ActionTurbine::ReservoirTropBas;
            }
            /* Rallumer EN VOL reste permis : c'est la procédure après une
               extinction, pas une fausse manoeuvre. */
        } else if (!m_inGroundContact) {
            /* Couper la turbine en vol, c'est se mettre en autorotation sans
               l'avoir voulu. Le vrai appareil a un robinet coupe-feu que rien
               n'empêche de fermer ; ici, une touche pressée par erreur ne doit
               pas terminer le vol. */
            return ActionTurbine::PasAuSol;
        }
        m_turbine.toggle();
        return ActionTurbine::Faite;
    }

    /* L'appareil touche le sol (patins posés ou en contact). */
    [[nodiscard]] bool auSol() const noexcept { return m_inGroundContact; }

    /* Vide une quantité de carburant hors consommation de la turbine : une
       cellule qui encaisse un choc perd du kérosène (voir le contact avec le sol
       du mode zombie). Le réservoir ne descend jamais sous zéro, et une quantité
       nulle ou négative ne fait rien. */
    void drainFuel(float liters) noexcept {
        if (liters > 0.0f) {
            m_fuelLiters = std::max(0.0f, m_fuelLiters - liters);
        }
    }

    /* Ajoute du carburant hors ravitaillement au sol (bidon ramassé en vol,
       mode zombie) : le réservoir ne dépasse jamais sa capacité, et une
       quantité nulle ou négative ne fait rien. */
    void addFuel(float liters) noexcept {
        if (liters > 0.0f) {
            m_fuelLiters = std::min(FUEL_CAPACITY_L, m_fuelLiters + liters);
        }
    }

    /* Pas collectif réel en degrés de pale (graduation du levier, voir
       PAS_MIN_DEG) : pour l'affichage HUD et la notice. */
    [[nodiscard]] float pasDeg() const noexcept { return m_pasDeg; }

    /* Surchauffe du régime transitoire (0 = régime continu, 1 = équilibre au
       plafond de 4070 m) : elle pousse la TMP vers 550 degrés et, au-delà de 1,
       fait fondre le plancher de puissance (voir POWER_FLAT_W). */
    [[nodiscard]] float surchauffe() const noexcept { return m_surchauffe; }

    /* Intensité du vortex ring state (0 = aucun, 1 = plein), pour l'alerte HUD.
       Toujours 0 en mode assisté et en démo (physique réelle coupée). */
    [[nodiscard]] float vrsIntensity() const noexcept { return m_vrsIntensity; }

    /* Présence dans la zone à éviter du diagramme hauteur-vitesse (0 = hors zone,
       1 = dedans, lissée sur ~2 s) : pour l'indicateur HUD. Toujours 0 turbine
       coupée, au sol, ou en physique assistée. */
    [[nodiscard]] float hvIntensity() const noexcept { return m_hvIntensity; }

    /* Intensité du décrochage de pale reculante (0 = aucun, 1 = franc), montante à
       l'approche de la VNE. Exposée comme le VRS pour qu'un jour le HUD puisse la
       signaler, et pour que les tests l'observent sans intégrer la dynamique.
       Toujours 0 en mode assisté et en démo (physique réelle coupée). */
    [[nodiscard]] float retreatingStall() const noexcept { return m_retreatingStall; }

    /* Vitesse d'arrivée (m/s) du dernier contact avec le sol, puis remise à zéro :
       l'appelant la lit une fois et la consomme. Elle ne peut se mesurer QU'ICI,
       le contact annulant aussitôt la composante verticale (voir update) ; la
       boucle de jeu, qui tourne bien plus lentement que la simulation, ne verrait
       plus qu'un appareil posé, vitesse nulle. Le mode zombie en tire le carburant
       que fait fuir un posé brutal. Vaut le maximum des contacts survenus depuis la dernière
       lecture, et reste à 0 tant que l'appareil demeure au sol. */
    [[nodiscard]] float consumeGroundImpact() noexcept {
        const float v    = m_groundImpactMs;
        m_groundImpactMs = 0.0f;
        return v;
    }

private:
    /* Après l'intégration : bride vitesses et rotations, applique le contact
       sol et le maintien sur les patins. Défini dans FlightModelContact.cpp. */
    void briderEtPoser() noexcept;

    /* Repositionner l'appareil ne doit pas laisser derrière lui un contact non
       lu : la partie suivante encaisserait les dégâts d'un posé qui n'a pas eu
       lieu. On oublie aussi l'état "au sol", le nouveau point de départ pouvant
       être en vol comme sur un pad. */
    void clearGroundImpact() noexcept {
        m_groundImpactMs  = 0.0f;
        m_inGroundContact = false;
    }

    /* Un repositionnement doit ramener le plan des pales au neutre : sinon
       l'appareil hériterait, au pas suivant, d'un reliquat de bascule qui ne
       correspond plus à la commande du pilote. */
    void clearRotorLag() noexcept {
        m_cyclicLateralLagged      = 0.0f;
        m_cyclicLongitudinalLagged = 0.0f;
    }

    RigidBody m_body;
    Turbine   m_turbine;
    float     m_lastThrust   = 0.0f;
    float     m_groundHeight = 0.0f;
    float     m_fuelLiters   = FUEL_CAPACITY_L;
    float     m_pasDeg     = PAS_MIN_DEG;      /* pas collectif réel, en degrés de pale */
    float     m_surchauffe = 0.0f;             /* état lent du régime transitoire, 0..~1,3 */
    float     m_vrsIntensity  = 0.0f;          /* vortex ring state, 0..1 (alerte HUD) */
    float     m_hvIntensity   = 0.0f;          /* zone hauteur-vitesse, 0..1 lissée (HUD) */
    float     m_retreatingStall = 0.0f;        /* décrochage de pale reculante, 0..1 */
    float     m_groundImpactMs = 0.0f;         /* vitesse du dernier contact, non lue */
    bool      m_inGroundContact = false;       /* déjà au sol au pas précédent */
    bool      m_realFlyPhysicsEnabled = true;  /* coupé en mode assisté et en démo */
    float     m_cyclicLateralLagged      = 0.0f;  /* bascule du plan de pales, retard gyroscopique */
    float     m_cyclicLongitudinalLagged = 0.0f;
};

/* Bille (inclinomètre) : force spécifique latérale, en g, positive à droite.
 * Pesanteur projetée sur l'axe droit du fuselage plus accélération du virage
 * omega x v, ce qui évite de dériver la vitesse image par image. L'axe droit est Z
 * (X est l'avant, Y le haut). Signe inversé : en virage mal coordonné la bille part
 * vers l'extérieur. */
[[nodiscard]] inline float billeG(const RigidBody& body) noexcept {
    const vec3  right  = body.orientation * vec3{0.0f, 0.0f, 1.0f};
    const vec3  omega  = body.orientation * body.angularVelocity;
    const float latMs2 = glm::dot(glm::cross(omega, body.velocity), right) + G * right.y;
    return -latMs2 / G;
}

/* Taux de virage en degrés par seconde, positif à droite. Rotation autour de la
 * verticale MONDE, pas de l'axe de lacet du fuselage, qui s'incline avec le roulis.
 * Le repère monde a Z vers le sud, donc virer à droite est négatif autour de Y. */
[[nodiscard]] inline float tauxVirageDegS(const RigidBody& body) noexcept {
    const vec3 omega = body.orientation * body.angularVelocity;
    return -glm::degrees(omega.y);
}

/* Couple de girouette en lacet, en N.m (voir KVANE). Une dérive vers la droite
 * (vitesse latérale positive) ramène le nez à droite, donc un couple négatif par la
 * convention du modèle. vitesseAvant est bornée à zéro par l'appelant : rien au
 * stationnaire ni en vol arrière. */
[[nodiscard]] inline float coupleGirouette(float vitesseAvant, float vitesseLaterale) noexcept {
    return -KVANE * vitesseAvant * vitesseLaterale;
}

}  /* namespace artouste::physics */
