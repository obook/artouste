/*
 * LandingAutopilot.cpp
 * Logique de l'atterrissage automatique (voir LandingAutopilot.hpp). Le guidage
 * horizontal et la finale sont ceux du retour au pad de DemoPilot (mêmes constantes,
 * DemoPilotDetail.hpp), appliqués ici à une cible unique au lieu d'une route.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/LandingAutopilot.hpp"

#include "app/DemoPilotDetail.hpp"
#include "physics/constants.hpp"

#include <cmath>

namespace artouste::app {

using namespace demo_detail;

namespace {
/* Lissage à l'engagement (voir m_cyclicLongitudinalOut et consorts,
 * LandingAutopilot.hpp) : ENGAGE_TAU adoucit la reprise en main juste après
 * start(), mais ne doit pas s'éterniser -- un guidage en permanence lissé
 * réagirait trop mollement plus tard dans l'approche (franchissement de relief,
 * rattrapage d'un excédent d'altitude). ENGAGE_DECAY fait donc décroître ce
 * lissage vers l'instantané au fil des secondes qui suivent l'engagement : au
 * bout de quelques ENGAGE_DECAY, le guidage a retrouvé sa pleine réactivité. */
constexpr float ENGAGE_TAU   = 1.2f;
constexpr float ENGAGE_DECAY = 1.0f;

/* Constante de temps effective du lissage, décroissante depuis ENGAGE_TAU
 * jusqu'à ~0 avec le temps écoulé depuis l'engagement. */
float tauEngagement(float tempsEcoule) noexcept {
    return ENGAGE_TAU * std::exp(-tempsEcoule / ENGAGE_DECAY);
}
}  // namespace

void LandingAutopilot::start(const vec3& target, const physics::Controls& initialControls) noexcept {
    m_active     = true;
    m_target     = target;
    m_collective = initialControls.collective;
    m_baseline   = initialControls;
    m_grounded   = false;

    /* Partir des commandes du pilote au moment de l'engagement plutôt que du
       neutre : la première commande calculée par le guidage se lisse depuis là où
       le manche se trouvait vraiment, pas depuis zéro. */
    m_cyclicLongitudinalOut = initialControls.cyclicLongitudinal;
    m_cyclicLateralOut      = initialControls.cyclicLateral;
    m_pedalsOut             = initialControls.pedals;
    m_tempsEngagement       = 0.0f;
}

float LandingAutopilot::rampeCollectif(float cible, float dt) noexcept {
    cible                = clamp(cible, 0.0f, COLLECTIF_MAX);
    const float pas      = COLLECTIF_RATE * dt;
    m_collective += clamp(cible - m_collective, -pas, pas);
    return m_collective;
}

physics::Controls LandingAutopilot::update(float dt, const vec3& position, const vec3& velocity,
                                           float heading, float agl,
                                           const std::function<float(float, float)>& terrainHeight) noexcept {
    physics::Controls out;
    if (!m_active) {
        return out;
    }

    /* Posé : commandes neutres, collectif ramené à zéro (plutôt que de rendre la
       main tout de suite au collectif de pose, proche de la sustentation) pour
       sortir franchement de l'effet de sol et ne pas redécoller tout seul. */
    if (m_grounded) {
        out.collective = rampeCollectif(0.0f, dt);
        if (out.collective <= 0.0f) {
            m_active = false;
        }
        return out;
    }

    m_tempsEngagement += dt;
    const float tau = tauEngagement(m_tempsEngagement);

    const float dx   = m_target.x - position.x;
    const float dz   = m_target.z - position.z;
    const float dist = std::sqrt(dx * dx + dz * dz);

    /* Guidage en cap, sauf tout près (voir DemoPilot::update). Lissé comme le
       cyclique ci-dessous (voir tauEngagement) : même souci de ne pas faire
       pivoter le nez d'un coup à l'engagement. */
    const float pedalsCible = (dist > DIST_CAP_MIN) ? palonnierVers(m_target, position, heading) : 0.0f;
    m_pedalsOut             = lowPass(m_pedalsOut, pedalsCible, dt, tau);
    out.pedals              = m_pedalsOut;

    /* Asservissement horizontal en vitesse, comme le retour au pad de la démo. Un
       filet de vitesse (vitesseMinApproche) maintient un peu d'avance tant que
       l'appareil est plus haut que la pente ne l'exige à cette distance, sinon il
       s'immobiliserait au-dessus du pad avant d'avoir fini de descendre. */
    const float vNorme       = std::max(clamp(GAIN_V_DIST * dist, 0.0f, V_APPROCHE_MAX),
                                        vitesseMinApproche(dist, agl));
    const vec3  versCible    = (dist > 0.001f) ? vec3{dx / dist, 0.0f, dz / dist} : vec3{0.0f};
    const vec3  vitesseCible = versCible * vNorme;
    const vec3  ecartV{vitesseCible.x - velocity.x, 0.0f, vitesseCible.z - velocity.z};
    const vec3  avant{std::cos(heading), 0.0f, -std::sin(heading)};
    const vec3  droite{std::sin(heading), 0.0f, std::cos(heading)};
    const float cyclicLongitudinalCible =
        clamp(GAIN_CYCLIQUE_LON * glm::dot(ecartV, avant), -CYCLIQUE_MAX_LON, CYCLIQUE_MAX_LON);
    const float cyclicLateralCible =
        clamp(GAIN_CYCLIQUE_LAT * glm::dot(ecartV, droite), -CYCLIQUE_MAX_LAT, CYCLIQUE_MAX_LAT);
    /* Lissage à l'engagement (voir tauEngagement, m_cyclicLongitudinalOut) : le
       manche rejoint la commande calculée en douceur plutôt que d'y sauter d'un
       coup. */
    m_cyclicLongitudinalOut = lowPass(m_cyclicLongitudinalOut, cyclicLongitudinalCible, dt, tau);
    m_cyclicLateralOut      = lowPass(m_cyclicLateralOut, cyclicLateralCible, dt, tau);
    out.cyclicLongitudinal  = m_cyclicLongitudinalOut;
    out.cyclicLateral       = m_cyclicLateralOut;

    /* Collectif : hauteur d'approche proportionnelle à la distance, avec un taux de
       descente plafonné sous le seuil de l'alerte GPWS du HUD (collectifApprocheGpws,
       comme le retour au pad de la démo), puis vitesse verticale tenue en finale
       (derniers mètres avant le pad). Un relief intermédiaire (col, flanc de montagne)
       peut imposer de monter plus haut que ne le ferait la seule pente d'approche :
       voir hauteurMinRelief. Non bornée par ALT_PLAFOND (qui limite la montée en
       transit pour rester réaliste, pas une contrainte de sécurité) : franchir le
       relief prime toujours sur rester bas. */
    float collectifCible;
    if (dist < DIST_POSE) {
        collectifCible = saturate(physics::COLL_HOVER + GAIN_VZ_POSE * (VZ_POSE - velocity.y));
    } else {
        const float hauteurPente  = clamp(GAIN_ALT_RETOUR * dist, 0.0f, ALT_PLAFOND);
        const float hauteurRelief = terrainHeight ? hauteurMinRelief(position, m_target, dist, terrainHeight)
                                                   : 0.0f;
        /* Pente référée au niveau du pad : un pad perché n'entre dans terrainHeight
           que dans ses 8 m (PAD_PLATFORM_RADIUS_M), donc une pente référée au sol
           local mène l'appareil sous le plateau, puis le contact l'y remet d'un coup. */
        const float solLocal   = position.y - agl;
        const float hauteurPad = (m_target.y - solLocal) + hauteurPente;
        float hauteurCible     = (hauteurRelief > hauteurPente) ? hauteurRelief : hauteurPente;
        if (hauteurPad > hauteurCible) {
            hauteurCible = hauteurPad;
        }
        collectifCible = collectifApprocheGpws(hauteurCible, agl, velocity.y);
    }
    out.collective = rampeCollectif(collectifCible, dt);

    /* Contact avec le pad : proche et au sol -> on entame la sortie d'effet de sol
       (voir m_grounded en tête de fonction, traité au tour suivant). */
    if (dist < DIST_POSE && agl < AGL_POSE) {
        m_grounded = true;
    }

    return out;
}

}  /* namespace artouste::app */
