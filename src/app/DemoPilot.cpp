/*
 * DemoPilot.cpp
 * Logique du pilote automatique de démonstration (voir DemoPilot.hpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/DemoPilot.hpp"

#include "physics/constants.hpp"

#include <cmath>

namespace artouste::app {

namespace {

/* --- Découpage du début de la démo ------------------------------------------- */
constexpr float ROTOR_PRET      = 0.99f; /* régime rotor à atteindre avant de décoller (plein régime) */
constexpr float DELAI_DECOLLAGE = 3.0f;  /* s : attente sur le pad après le plein régime rotor avant de décoller */
constexpr float DUREE_MONTEE    = 5.0f;  /* s : décollage vertical avant de partir vers la dune */
constexpr float COLLECTIF_RATE  = 0.35f; /* 1/s : vitesse de variation du collectif (levier monté en douceur) */
constexpr float DELAI_REDEMARRAGE = 5.0f;/* s : attente après l'arrêt des pales avant de relancer la démo */
constexpr float T_MAX        = 720.0f; /* garde-fou : on relance la démo au plus tard à cet instant (12 min) */

/* --- Réglages du vol --------------------------------------------------------- */
constexpr float ALT_SURVOL  = 1500.0f; /* hauteur de survol de la Dune du Pilat (m) */
constexpr float V_CROISIERE = 35.0f;   /* vitesse de croisière visée (m/s) : assiette de croisière réaliste (~10 deg) */
constexpr float RAYON_DUNE  = 300.0f;  /* distance à la dune en deçà de laquelle on entame le demi-tour (m) */

/* --- Gains du guidage proportionnel ------------------------------------------ */
constexpr float GAIN_CAP        = 1.4f;    /* palonnier par radian d'erreur de cap */
constexpr float CAP_MAX         = 0.7f;    /* palonnier maximal (évite de pivoter trop vite) */
constexpr float GAIN_ALT        = 0.020f;  /* collectif par mètre d'erreur d'altitude */
constexpr float GAIN_VZ         = 0.04f;   /* amortissement par la vitesse verticale (m/s) */
constexpr float GAIN_V_DIST     = 0.14f;   /* vitesse visée (m/s) par mètre de distance à la cible */
constexpr float GAIN_CYCLIQUE   = 0.08f;   /* cyclique par (m/s) d'écart de vitesse */
constexpr float CYCLIQUE_MAX    = 0.45f;   /* cyclique maximal : borne l'inclinaison à une assiette réaliste */
constexpr float GAIN_ALT_RETOUR = 0.20f;   /* hauteur visée (m) par mètre de distance au pad (descente du retour) */
constexpr float DIST_CAP_MIN    = 30.0f;   /* en deçà, on ne pivote plus le nez (la cible est trop proche) */
constexpr float VZ_POSE         = -0.4f;   /* m/s : vitesse de descente visée pour une pose très douce */
constexpr float GAIN_VZ_POSE    = 0.15f;   /* collectif par (m/s) d'écart de vitesse verticale, à la pose */

/* --- Détection de la pose ---------------------------------------------------- */
constexpr float DIST_POSE = 15.0f;  /* distance horizontale au pad sous laquelle on est "arrivé" (m) */
constexpr float AGL_POSE  = 1.0f;   /* hauteur-sol sous laquelle on considère l'appareil posé (m) */

/* Ramène un angle dans l'intervalle [-PI, +PI]. */
float wrapPi(float a) noexcept {
    a = std::fmod(a + PI, TWO_PI);
    if (a < 0.0f) {
        a += TWO_PI;
    }
    return a - PI;
}

/* Cap (rad) d'un vecteur monde horizontal, même convention que l'application :
   atan2(-z, x), donc 0 vers l'est, +PI/2 vers le nord. */
float bearing(float dx, float dz) noexcept {
    return std::atan2(-dz, dx);
}

/* Palonnier pour tourner le nez vers la cible (guidage en cap). Un cap visé plus à
   gauche (erreur positive) demande un palonnier négatif, car le palonnier droit
   (positif) fait partir le nez à droite et diminue le cap. */
float palonnierVers(const vec3& cible, const vec3& pos, float cap) noexcept {
    const float vise   = bearing(cible.x - pos.x, cible.z - pos.z);
    const float erreur = wrapPi(vise - cap);
    return clamp(-GAIN_CAP * erreur, -CAP_MAX, CAP_MAX);
}

/* Collectif pour rejoindre et tenir une hauteur-sol cible, amorti par la vitesse
   verticale pour ne pas osciller. Centré sur le collectif de sustentation. */
float collectifPour(float hauteurCible, float hauteurSol, float vitesseVerticale) noexcept {
    const float corr = GAIN_ALT * (hauteurCible - hauteurSol) - GAIN_VZ * vitesseVerticale;
    return saturate(physics::COLL_HOVER + corr);
}

}  /* namespace */

void DemoPilot::start(const vec3& returnPad, const vec3& dunePoint) noexcept {
    m_active         = true;
    m_elapsed        = 0.0f;
    m_rotorReadyTime = -1.0f;  /* on attendra le plein régime rotor avant de décoller */
    m_retour         = false;  /* on commence par l'aller, vers la dune */
    m_collective     = 0.0f;   /* levier au repos */
    m_landed         = false;
    m_turbineCut     = false;
    m_stoppedTime    = -1.0f;
    m_returnPad      = returnPad;
    m_dunePoint      = dunePoint;
}

float DemoPilot::rampeCollectif(float cible, float dt) noexcept {
    const float pas = COLLECTIF_RATE * dt;  /* variation maximale sur ce pas de temps */
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
        /* Phase 3 : décollage vertical pour quitter le pad, sans avancer encore. Le
           collectif est monté en douceur (rampe) pour un décollage progressif. */
        out.viewMode            = 0;  /* poursuite */
        out.controls.collective = rampeCollectif(collectifPour(ALT_SURVOL, agl, velocity.y), dt);
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

    /* Phase 3 : vol guidé vers la cible courante. À l'aller, on vise la Dune du Pilat
       en montant à l'altitude de survol ; arrivé au-dessus, on fait demi-tour. Au
       retour, on vise le pad de départ en descendant pour s'y poser. */
    vec3  cible;
    float hauteurCible;
    if (!m_retour) {
        cible        = m_dunePoint;
        hauteurCible = ALT_SURVOL;  /* on monte jusqu'à la hauteur de survol */
    } else {
        cible        = m_returnPad;
        const float dPad = std::sqrt((m_returnPad.x - position.x) * (m_returnPad.x - position.x) +
                                     (m_returnPad.z - position.z) * (m_returnPad.z - position.z));
        /* Descente proportionnelle à la distance au pad. Tout près du pad (pose), c'est
           un asservissement en vitesse verticale qui prend le relais (voir plus bas). */
        hauteurCible = clamp(GAIN_ALT_RETOUR * dPad, 0.0f, ALT_SURVOL);
    }

    const float dx   = cible.x - position.x;
    const float dz   = cible.z - position.z;
    const float dist = std::sqrt(dx * dx + dz * dz);  /* distance horizontale à la cible (m) */

    /* Aller : une fois au-dessus de la dune, on bascule en retour. */
    if (!m_retour && dist < RAYON_DUNE) {
        m_retour = true;
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
    if (m_retour && dist < DIST_POSE) {
        collectifCible = saturate(physics::COLL_HOVER + GAIN_VZ_POSE * (VZ_POSE - velocity.y));
    } else {
        collectifCible = collectifPour(hauteurCible, agl, velocity.y);
    }
    out.controls.collective = rampeCollectif(collectifCible, dt);

    /* Vues : variété en route (cockpit / orbite par bandes de temps), poursuite à
       l'approche du pad au retour, orbite pour la pose. */
    if (m_retour && dist < 60.0f) {
        out.viewMode = 2;  /* pose : vue d'orbite */
    } else if (m_retour && dist < 200.0f) {
        out.viewMode = 0;  /* approche : vue de poursuite */
    } else {
        /* En route : on fait défiler les trois vues à tour de rôle (poursuite,
           cockpit, orbite), une dizaine de secondes chacune, pour varier les angles. */
        const float bande = tVol - DUREE_MONTEE;
        out.viewMode = static_cast<int>(bande / 10.0f) % 3;  /* 0 -> 1 -> 2 -> 0 ... */
    }

    /* Détection de la pose : au retour, près du pad et au sol -> on entame la séquence
       d'arrêt (traitée en tête de update au tour suivant). Le garde-fou de durée relance
       directement la démo si le vol s'éternise. */
    if (m_retour && dist < DIST_POSE && agl < AGL_POSE) {
        m_landed = true;
    }
    if (m_elapsed > T_MAX) {
        out.finished = true;
    }

    return out;
}

}  /* namespace artouste::app */
