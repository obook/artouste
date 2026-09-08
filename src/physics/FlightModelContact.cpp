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

void FlightModel::briderEtPoser(float dt) noexcept {
    /* --- Garde-fous numériques ------------------------------------ */
    /* On limite vitesses et rotations pour que la simulation reste stable. */
    m_body.velocity.x = clampAbs(m_body.velocity.x, MAX_SPEED);
    m_body.velocity.y = clampAbs(m_body.velocity.y, MAX_SPEED);
    m_body.velocity.z = clampAbs(m_body.velocity.z, MAX_SPEED);
    m_body.angularVelocity.x = clampAbs(m_body.angularVelocity.x, MAX_OMEGA);
    m_body.angularVelocity.y = clampAbs(m_body.angularVelocity.y, MAX_OMEGA);
    m_body.angularVelocity.z = clampAbs(m_body.angularVelocity.z, MAX_OMEGA);

    /* Vitesse de rapprochement du sol : de combien la garde au sol a baissé
       pendant ce pas. Au-dessus d'un terrain plat, c'est le taux de chute ; elle
       monte toute seule quand le relief se soulève sous l'appareil, donc rentrer
       dans un versant à plat reste un choc. On prenait avant la vitesse
       complète : frôler une plaine à 40 m/s coûtait le prix d'un crash.
       Plafonnée à la vitesse de l'appareil, sinon un saut de m_groundHeight
       (changement de carte) passerait pour un choc. */
    const float clearance   = m_body.position.y - m_groundHeight;
    const float rapprochement =
        (dt > 0.0f) ? std::min((m_clearancePrev - clearance) / dt, glm::length(m_body.velocity))
                    : 0.0f;

    /* Contact avec le sol : l'appareil ne descend pas sous le relief. */
    if (m_body.position.y < m_groundHeight) {
        /* Relevée avant d'annuler la vitesse verticale, sinon il n'en reste
           rien. Seul le pas qui entre en contact compte : un appareil posé se
           blesserait sinon à chaque pas. */
        if (!m_inGroundContact) {
            m_groundImpactMs = std::max(m_groundImpactMs, std::max(0.0f, rapprochement));
        }
        m_body.position.y = m_groundHeight;
        if (m_body.velocity.y < 0.0f) {
            m_body.velocity.y = 0.0f;
        }
    }
    m_inGroundContact = m_body.position.y <= m_groundHeight;
    m_clearancePrev   = m_body.position.y - m_groundHeight;

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
