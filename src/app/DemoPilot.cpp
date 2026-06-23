/*
 * DemoPilot.cpp
 * Logique du pilote automatique de démonstration (voir DemoPilot.hpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/DemoPilot.hpp"

#include "app/DemoPilotDetail.hpp"
#include "physics/constants.hpp"

#include <cmath>

namespace artouste::app {

using namespace demo_detail;

void DemoPilot::start(const vec3& returnPad, const std::vector<Waypoint>& route) noexcept {
    m_active         = true;
    m_elapsed        = 0.0f;
    m_rotorReadyTime = -1.0f;  /* on attendra le plein régime rotor avant de décoller */
    m_index          = 0;      /* on commence par le premier point de passage */
    m_collective     = 0.0f;   /* levier au repos */
    m_landed         = false;
    m_turbineCut     = false;
    m_stoppedTime    = -1.0f;
    m_returnPad      = returnPad;
    m_route          = route;
    /* Route vide : pas de point à survoler, on passe directement au retour (décollage
       puis pose sur le pad). */
    m_returning      = m_route.empty();
}

float DemoPilot::rampeCollectif(float cible, float dt) noexcept {
    cible = clamp(cible, 0.0f, COLLECTIF_MAX);  /* limite la température de tuyère en montée */
    const float pas = COLLECTIF_RATE * dt;      /* variation maximale sur ce pas de temps */
    m_collective += clamp(cible - m_collective, -pas, pas);
    return m_collective;
}

DemoPilot::Output DemoPilot::update(float dt, const vec3& position, const vec3& velocity,
                                    float heading, float groundHeight,
                                    float rotorFraction) noexcept {
    Output out;
    if (!m_active) {
        return out;
    }

    m_elapsed += dt;
    const float agl = position.y - groundHeight;  /* hauteur au-dessus du sol (m) */

    /* Phase 1 : posé sur le pad, on attend que le rotor ait atteint son plein régime
       (la turbine monte en régime via le démarrage rapide déclenché par l'application).
       Tirer sur le collectif avant aurait fait décoller l'appareil pales pas encore
       lancées. Vue d'orbite pour montrer les pales qui s'élancent. */
    if (rotorFraction < ROTOR_PRET && m_rotorReadyTime < 0.0f) {
        out.viewMode = 2;
        return out;  /* collectif laissé à 0 : l'appareil reste collé au pad */
    }
    if (m_rotorReadyTime < 0.0f) {
        m_rotorReadyTime = m_elapsed;  /* instant où le rotor a atteint son régime */
    }

    /* Phase 2 : rotor au plein régime, on patiente quelques secondes sur le pad avant
       de décoller (départ posé, comme un vrai pilote qui stabilise avant de monter). */
    const float depuisRegime = m_elapsed - m_rotorReadyTime;
    if (depuisRegime < DELAI_DECOLLAGE) {
        out.viewMode = 2;  /* toujours sur le pad, vue d'orbite */
        return out;        /* collectif laissé à 0 */
    }
    const float tVol = depuisRegime - DELAI_DECOLLAGE;  /* temps écoulé depuis le décollage */

    if (tVol < DUREE_MONTEE) {
        /* Phase 3 : décollage vertical pour quitter le pad, sans avancer encore. On
           asservit la vitesse de montée à une valeur douce (collectifDecollage) plutôt
           que de tirer le collectif à fond : l'appareil s'élève lentement, en douceur.
           La rampe (rampeCollectif) lisse encore la mise en action du levier. */
        out.viewMode            = 0;  /* poursuite */
        out.controls.collective = rampeCollectif(collectifDecollage(velocity.y), dt);
        return out;
    }

    /* Phase d'arrêt : une fois posé, commandes neutres (l'appareil reste au sol),
       coupure de la turbine, puis attente de l'arrêt complet des pales (par la roue
       libre) et de quelques secondes avant de relancer la démo. Conforme à la
       procédure : collectif à zéro, coupure carburant, le rotor ralentit puis s'arrête. */
    if (m_landed) {
        out.viewMode = 2;  /* orbite : on voit le rotor ralentir puis s'immobiliser */
        if (!m_turbineCut) {
            out.cutTurbine = true;  /* demande à l'application de couper la turbine (une fois) */
            m_turbineCut   = true;
        }
        if (rotorFraction <= 0.0f) {        /* pales arrêtées */
            if (m_stoppedTime < 0.0f) {
                m_stoppedTime = m_elapsed;  /* instant de l'arrêt complet du rotor */
            }
            if (m_elapsed - m_stoppedTime >= DELAI_REDEMARRAGE) {
                out.finished = true;        /* 5 s écoulées : on relance la démo */
            }
        }
        return out;  /* commandes neutres : collectif 0, l'appareil reste posé */
    }

    /* Phase 3 : vol guidé vers le point courant. Tant qu'il reste des points de passage,
       on vise le point courant à sa hauteur de survol ; une fois assez proche, on vise le
       suivant. Après le dernier point, on vise le pad de départ en descendant pour s'y
       poser. */
    vec3  cible;
    float hauteurCible;
    if (!m_returning) {
        cible        = m_route[m_index].point;
        hauteurCible = m_route[m_index].altitude;  /* hauteur de survol propre à ce point */
    } else {
        cible        = m_returnPad;
        const float dPad = std::sqrt((m_returnPad.x - position.x) * (m_returnPad.x - position.x) +
                                     (m_returnPad.z - position.z) * (m_returnPad.z - position.z));
        /* Descente proportionnelle à la distance au pad. Tout près du pad (pose), c'est
           un asservissement en vitesse verticale qui prend le relais (voir plus bas). */
        hauteurCible = clamp(GAIN_ALT_RETOUR * dPad, 0.0f, ALT_PLAFOND);
    }

    const float dx   = cible.x - position.x;
    const float dz   = cible.z - position.z;
    const float dist = std::sqrt(dx * dx + dz * dz);  /* distance horizontale à la cible (m) */

    /* Une fois assez proche du point courant, on passe au suivant ; après le dernier, on
       entame le retour vers le pad. */
    if (!m_returning && dist < RAYON_POINT) {
        ++m_index;
        if (m_index >= m_route.size()) {
            m_returning = true;
        }
    }

    /* On garde le nez pointé vers la cible, sauf tout près (le cap n'aurait plus de
       sens et l'appareil se mettrait à tourner sur lui-même). */
    if (dist > DIST_CAP_MIN) {
        out.controls.pedals = palonnierVers(cible, position, heading);
    }

    /* Asservissement horizontal en vitesse : vitesse visée dirigée vers la cible, dont
       la norme décroît avec la distance (pleine croisière au loin, nulle au but).
       L'écart avec la vitesse réelle est ramené dans le repère de l'appareil pour
       commander le cyclique : composante avant -> tangage, droite -> roulis. En
       annulant le vecteur vitesse au-dessus du pad, l'appareil s'arrête pour se poser
       au lieu de tourner autour. */
    const vec3  versCible    = (dist > 0.001f) ? vec3{dx / dist, 0.0f, dz / dist} : vec3{0.0f};
    const vec3  vitesseCible = versCible * clamp(GAIN_V_DIST * dist, 0.0f, V_CROISIERE);
    const vec3  ecartV{vitesseCible.x - velocity.x, 0.0f, vitesseCible.z - velocity.z};
    const vec3  avant{std::cos(heading), 0.0f, -std::sin(heading)};  /* nez, au sol */
    const vec3  droite{std::sin(heading), 0.0f, std::cos(heading)};  /* flanc droit, au sol */
    out.controls.cyclicLongitudinal =
        clamp(GAIN_CYCLIQUE * glm::dot(ecartV, avant), -CYCLIQUE_MAX, CYCLIQUE_MAX);
    out.controls.cyclicLateral =
        clamp(GAIN_CYCLIQUE * glm::dot(ecartV, droite), -CYCLIQUE_MAX, CYCLIQUE_MAX);

    /* Collectif : en vol, on suit la hauteur visée ; tout près du pad au retour, on
       passe à une descente douce à vitesse verticale contrôlée (~0,4 m/s), pour une
       pose très douce et robuste à l'effet de sol (réduction de collectif en finale,
       conforme à la procédure). */
    float collectifCible;
    if (m_returning && dist < DIST_POSE) {
        collectifCible = saturate(physics::COLL_HOVER + GAIN_VZ_POSE * (VZ_POSE - velocity.y));
    } else {
        collectifCible = collectifPour(hauteurCible, agl, velocity.y);
    }
    out.controls.collective = rampeCollectif(collectifCible, dt);

    /* Vues : variété en route (cockpit / orbite par bandes de temps), poursuite à
       l'approche du pad au retour, orbite pour la pose. */
    if (m_returning && dist < 60.0f) {
        out.viewMode = 2;  /* pose : vue d'orbite */
    } else if (m_returning && dist < 200.0f) {
        out.viewMode = 0;  /* approche : vue de poursuite */
    } else {
        /* En route : on fait défiler les trois vues. L'orbite, plus courte, laisse la
           caméra faire un tour complet (durée à garder en phase avec DEMO_ORBIT_TURN
           côté application). */
        const float bande = std::fmod(tVol - DUREE_MONTEE, CYCLE_VUES);
        if (bande < DUREE_POURSUITE) {
            out.viewMode = 0;  /* poursuite */
        } else if (bande < DUREE_POURSUITE + DUREE_COCKPIT) {
            out.viewMode = 1;  /* cockpit */
        } else {
            out.viewMode = 2;  /* orbite (un tour complet) */
        }
    }

    /* HUD : il change à chaque cycle de vues et boucle : aucun, puis complet, puis
       quatre coins. (Pendant le démarrage, la montée et l'arrêt, il reste à 0.) */
    out.hudStyle = static_cast<int>((tVol - DUREE_MONTEE) / CYCLE_VUES) % 3;

    /* Détection de la pose : au retour, près du pad et au sol -> on entame la séquence
       d'arrêt (traitée en tête de update au tour suivant). Le garde-fou de durée relance
       directement la démo si le vol s'éternise. */
    if (m_returning && dist < DIST_POSE && agl < AGL_POSE) {
        m_landed = true;
    }
    if (m_elapsed > T_MAX) {
        out.finished = true;
    }

    return out;
}

}  /* namespace artouste::app */
