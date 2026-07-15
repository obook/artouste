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

void LandingAutopilot::start(const vec3& target, const physics::Controls& initialControls) noexcept {
    m_active     = true;
    m_target     = target;
    m_collective = initialControls.collective;
    m_baseline   = initialControls;
    m_grounded   = false;
}

float LandingAutopilot::rampeCollectif(float cible, float dt) noexcept {
    cible                = clamp(cible, 0.0f, COLLECTIF_MAX);
    const float pas      = COLLECTIF_RATE * dt;
    m_collective += clamp(cible - m_collective, -pas, pas);
    return m_collective;
}

physics::Controls LandingAutopilot::update(float dt, const vec3& position, const vec3& velocity,
                                           float heading, float agl) noexcept {
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

    const float dx   = m_target.x - position.x;
    const float dz   = m_target.z - position.z;
    const float dist = std::sqrt(dx * dx + dz * dz);

    /* Guidage en cap, sauf tout près (voir DemoPilot::update). */
    if (dist > DIST_CAP_MIN) {
        out.pedals = palonnierVers(m_target, position, heading);
    }

    /* Asservissement horizontal en vitesse, comme le retour au pad de la démo. Un
       filet de vitesse (vitesseMinApproche) maintient un peu d'avance tant que
       l'appareil est plus haut que la pente ne l'exige à cette distance, sinon il
       s'immobiliserait au-dessus du pad avant d'avoir fini de descendre. */
    const float vNorme       = std::max(clamp(GAIN_V_DIST * dist, 0.0f, V_CROISIERE),
                                        vitesseMinApproche(dist, agl));
    const vec3  versCible    = (dist > 0.001f) ? vec3{dx / dist, 0.0f, dz / dist} : vec3{0.0f};
    const vec3  vitesseCible = versCible * vNorme;
    const vec3  ecartV{vitesseCible.x - velocity.x, 0.0f, vitesseCible.z - velocity.z};
    const vec3  avant{std::cos(heading), 0.0f, -std::sin(heading)};
    const vec3  droite{std::sin(heading), 0.0f, std::cos(heading)};
    out.cyclicLongitudinal =
        clamp(GAIN_CYCLIQUE * glm::dot(ecartV, avant), -CYCLIQUE_MAX, CYCLIQUE_MAX);
    out.cyclicLateral =
        clamp(GAIN_CYCLIQUE * glm::dot(ecartV, droite), -CYCLIQUE_MAX, CYCLIQUE_MAX);

    /* Collectif : hauteur d'approche proportionnelle à la distance, avec un taux de
       descente plafonné sous le seuil de l'alerte GPWS du HUD (collectifApprocheGpws,
       comme le retour au pad de la démo), puis vitesse verticale tenue en finale
       (derniers mètres avant le pad). */
    float collectifCible;
    if (dist < DIST_POSE) {
        collectifCible = saturate(physics::COLL_HOVER + GAIN_VZ_POSE * (VZ_POSE - velocity.y));
    } else {
        const float hauteurCible = clamp(GAIN_ALT_RETOUR * dist, 0.0f, ALT_PLAFOND);
        collectifCible            = collectifApprocheGpws(hauteurCible, agl, velocity.y);
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
