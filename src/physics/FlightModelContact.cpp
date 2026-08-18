/*
 * FlightModelContact.cpp
 * Garde-fous numériques et contact avec le sol, appliqués après l'intégration.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "physics/FlightModel.hpp"

#include "physics/constants.hpp"

#include <algorithm>
#include <cmath>

namespace artouste::physics {

namespace {

/* Borne une valeur en valeur absolue. */
[[nodiscard]] float clampAbs(float v, float limit) noexcept {
    return v > limit ? limit : (v < -limit ? -limit : v);
}

} /* namespace */

void FlightModel::briderEtPoser() noexcept {
    /* --- Garde-fous numériques ------------------------------------ */
    /* On limite vitesses et rotations pour que la simulation reste stable. */
    m_body.velocity.x = clampAbs(m_body.velocity.x, MAX_SPEED);
    m_body.velocity.y = clampAbs(m_body.velocity.y, MAX_SPEED);
    m_body.velocity.z = clampAbs(m_body.velocity.z, MAX_SPEED);
    m_body.angularVelocity.x = clampAbs(m_body.angularVelocity.x, MAX_OMEGA);
    m_body.angularVelocity.y = clampAbs(m_body.angularVelocity.y, MAX_OMEGA);
    m_body.angularVelocity.z = clampAbs(m_body.angularVelocity.z, MAX_OMEGA);

    /* Contact avec le sol : l'appareil ne descend pas sous le relief. */
    if (m_body.position.y < m_groundHeight) {
        /* Vitesse d'arrivée, relevée AVANT d'annuler la composante verticale --
           après, il n'en reste rien. On prend la vitesse complète et non le seul
           taux de chute : rentrer dans un versant à l'horizontale reste un
           contact avec le sol. Seul le pas qui ENTRE en contact compte, sans quoi
           un appareil posé se blesserait à chaque pas de simulation. */
        if (!m_inGroundContact) {
            m_groundImpactMs = std::max(m_groundImpactMs, glm::length(m_body.velocity));
        }
        m_body.position.y = m_groundHeight;
        if (m_body.velocity.y < 0.0f) {
            m_body.velocity.y = 0.0f;
        }
    }
    m_inGroundContact = m_body.position.y <= m_groundHeight;

    /* Posé sur les patins : tant que la poussée ne dépasse pas le poids, l'appareil
     * reste collé au sol, sans glisser ni tourner. Dès que le collectif suffit à
     * le soulever, il décolle normalement. */
    if (m_body.position.y <= m_groundHeight && m_lastThrust <= MASS * G) {
        m_body.position.y      = m_groundHeight;
        m_body.velocity        = vec3{0.0f, 0.0f, 0.0f};
        m_body.angularVelocity = vec3{0.0f, 0.0f, 0.0f};
    }
}

} /* namespace artouste::physics */
