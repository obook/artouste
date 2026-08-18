/*
 * RocketSystemVues.cpp
 * Ce que le système de roquettes donne au rendu : roquettes en vol, explosions
 * en cours, traces de brûlure au sol.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/combat/RocketSystem.hpp"

#include "app/combat/RocketSystemReglages.hpp"

#include <cmath>

namespace artouste::app {

std::vector<RocketSystem::RocketView> RocketSystem::rockets() const {
    std::vector<RocketView> out;
    out.reserve(m_rockets.size());
    for (const Rocket& r : m_rockets) {
        const float speed = glm::length(r.velocity);
        const vec3  dir   = speed > 1e-4f ? r.velocity / speed : vec3{1.0f, 0.0f, 0.0f};
        out.push_back(RocketView{r.position, r.position - dir * ROCKET_TRAIL_M});
    }
    return out;
}

void RocketSystem::addExplosion(const vec3& center) {
    m_explosions.push_back(Explosion{center, 0.0f});
}

std::vector<RocketSystem::ExplosionView> RocketSystem::explosions() const {
    std::vector<ExplosionView> out;
    out.reserve(m_explosions.size());
    for (const Explosion& e : m_explosions) {
        ExplosionView v;
        v.center   = e.center;
        v.progress = std::min(1.0f, e.age / EXPLOSION_DURATION_S);
        out.push_back(v);
    }
    return out;
}

std::vector<RocketSystem::ScorchView> RocketSystem::scorches() const {
    std::vector<ScorchView> out;
    out.reserve(m_scorches.size());
    for (const Scorch& s : m_scorches) {
        ScorchView v;
        v.center = s.center;
        /* Pleine au début puis fondu linéaire jusqu'a 0 sur la durée de vie. */
        v.alpha      = 1.0f - s.age / SCORCH_DURATION_S;
        v.radius     = s.radius;
        v.elongation = s.elongation;
        v.yaw        = s.yaw;
        out.push_back(v);
    }
    return out;
}

} /* namespace artouste::app */
