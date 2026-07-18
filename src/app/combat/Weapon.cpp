/*
 * Weapon.cpp
 * Voir Weapon.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/combat/Weapon.hpp"

namespace artouste::app {

namespace {
constexpr float FIRE_RATE_HZ      = 4.0f;   /* roquettes par seconde, gâchette tenue */
constexpr float RELOAD_DURATION_S = 1.2f;   /* pause avant chargeur plein à nouveau */
}  /* namespace */

Weapon::FireResult Weapon::update(float dt, bool triggerHeld) noexcept {
    FireResult result;

    if (m_fireCooldownS > 0.0f) {
        m_fireCooldownS -= dt;
    }

    /* Chargeur vide : recharge automatique, aucun tir possible en attendant.
       Une fois écoulée, le chargeur repart au plein -- munitions infinies. */
    if (m_ammo <= 0) {
        m_reloadTimer -= dt;
        if (m_reloadTimer <= 0.0f) {
            m_ammo        = AMMO_MAX;
            m_reloadTimer = 0.0f;
        }
        return result;
    }

    if (!triggerHeld || m_fireCooldownS > 0.0f) {
        return result;
    }

    result.fired    = true;
    m_fireCooldownS = 1.0f / FIRE_RATE_HZ;
    --m_ammo;
    if (m_ammo <= 0) {
        m_reloadTimer = RELOAD_DURATION_S;
    }
    return result;
}

}  /* namespace artouste::app */
