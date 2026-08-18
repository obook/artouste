/*
 * ZombieHordeInstances.cpp
 * Ce que la horde donne au rendu : matrices d'instance, teintes d'yeux,
 * éclats de coup, positions et espèces.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/combat/ZombieHorde.hpp"

#include "app/combat/ZombieHordeReglages.hpp"

#include <cmath>

namespace artouste::app {

mat4 ZombieHorde::instanceMatrix(const Zombie& z) const {
    mat4 m;
    if (z.state == State::Dying) {
        /* Bascule progressivement en avant (chute) : anim procédurale simple,
           pas de ragdoll physique -- suffisant pour un zombie qui despawn
           peu après (voir DEATH_ANIM_DURATION_S). */
        const float progress = 1.0f - saturate(z.stateTimer / DEATH_ANIM_DURATION_S);
        m = glm::translate(mat4(1.0f), z.position - vec3{0.0f, progress * 0.4f, 0.0f});
        m = glm::rotate(m, z.yaw, vec3{0.0f, 1.0f, 0.0f});
        m = glm::rotate(m, progress * HALF_PI, vec3{1.0f, 0.0f, 0.0f});
    } else {
        const float bob =
            std::sin(m_time * IDLE_BOB_FREQ_HZ * TWO_PI + z.phase) * IDLE_BOB_AMPLITUDE_M;
        m = glm::translate(mat4(1.0f), z.position + vec3{0.0f, bob, 0.0f});
        m = glm::rotate(m, z.yaw, vec3{0.0f, 1.0f, 0.0f});
    }
    if (z.scale != 1.0f) {
        m = glm::scale(m, vec3{z.scale});  /* largueur : même modèle, agrandi */
    }
    return m;
}

std::vector<mat4> ZombieHorde::buildInstanceMatrices() const {
    std::vector<mat4> out;
    out.reserve(m_zombies.size());
    for (const Zombie& z : m_zombies) {
        if (z.state == State::Dead) {
            continue;
        }
        out.push_back(instanceMatrix(z));
    }
    return out;
}

std::vector<ZombieHorde::EyeTint> ZombieHorde::buildEyeTints() const {
    std::vector<EyeTint> out;
    out.reserve(m_zombies.size());
    for (const Zombie& z : m_zombies) {
        if (z.state == State::Dead) {
            continue;
        }
        /* Le regard s'éteint pendant la chute : la lueur suit la disparition du
           zombie au lieu de rester allumée sur un corps à terre. */
        const float fade = z.state == State::Dying ? saturate(z.stateTimer / DEATH_ANIM_DURATION_S)
                                                   : 1.0f;
        EyeTint    tint;
        const bool auLargueur = z.type == Type::Brood || z.fromBrood;
        tint.color  = (auLargueur ? EYE_COLOR_BROOD : EYE_COLOR_WALKER) * fade;
        tint.radius = EYE_RADIUS_M * z.scale;
        out.push_back(tint);
    }
    return out;
}

std::vector<float> ZombieHorde::buildHitFlashes() const {
    std::vector<float> out;
    out.reserve(m_zombies.size());
    for (const Zombie& z : m_zombies) {
        if (z.state == State::Dead) {
            continue;
        }
        out.push_back(saturate(z.hitFlashTimer / HIT_FLASH_DURATION_S));
    }
    return out;
}

std::vector<vec3> ZombieHorde::alivePositions() const {
    std::vector<vec3> out;
    out.reserve(m_zombies.size());
    for (const Zombie& z : m_zombies) {
        if (z.state != State::Dead) {
            out.push_back(z.position);
        }
    }
    return out;
}

std::vector<int> ZombieHorde::buildKinds() const {
    std::vector<int> out;
    out.reserve(m_zombies.size());
    for (const Zombie& z : m_zombies) {
        if (z.state == State::Dead) {
            continue;  /* même filtrage que buildInstanceMatrices */
        }
        out.push_back(static_cast<int>(z.kind));
    }
    return out;
}

} /* namespace artouste::app */
