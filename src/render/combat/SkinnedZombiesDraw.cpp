/*
 * SkinnedZombiesDraw.cpp
 * Pose des os, ancrage des yeux, instances et dessin de la horde.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/combat/SkinnedZombies.hpp"

#include "render/combat/SkinnedZombiesReglages.hpp"

#include "render/Shader.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>

namespace artouste::render {

std::size_t SkinnedZombies::bucketIndex(int kind) const noexcept {
    const auto        k = static_cast<unsigned int>(kind);
    const std::size_t v = k % m_parts.size();
    const std::size_t g = (k / GROUP_DECORRELATE) % static_cast<std::size_t>(m_phaseGroups);
    return v * static_cast<std::size_t>(m_phaseGroups) + g;
}

std::vector<mat4> SkinnedZombies::posedBones(std::size_t variant, const std::vector<mat4>& globals,
                                             float tg) const {
    /* Compensation du root motion : on retranche la dérive horizontale du
       centre pour épingler le zombie sur sa position logique (pas de
       glissement). Appliquée a gauche de chaque os = translation de la
       sortie skinnée avant la matrice d'instance. */
    const vec2 drift = m_model.rootDriftXZ(variant, tg);
    const mat4 corr  = glm::translate(mat4(1.0f), vec3{-drift.x, 0.0f, -drift.y});
    std::vector<mat4> bones = m_model.boneMatrices(variant, globals);
    for (mat4& b : bones) {
        b = corr * b;
    }
    return bones;
}

void SkinnedZombies::storeEyePoints(std::size_t bucket, std::size_t variant,
                                    const std::vector<mat4>& bones) {
    vec3 left{0.0f};
    vec3 right{0.0f};
    if (m_model.eyePoints(variant, bones, left, right)) {
        m_eyePoints[bucket] = {left, right};
    }
}

bool SkinnedZombies::eyeAnchors(int kind, vec3& left, vec3& right) const {
    if (!m_built || m_parts.empty()) {
        return false;
    }
    const std::pair<vec3, vec3>& yeux = m_eyePoints[bucketIndex(kind)];
    if (yeux.first == vec3(0.0f) && yeux.second == vec3(0.0f)) {
        return false;  /* variante sans os de tête repéré au chargement */
    }
    left  = yeux.first;
    right = yeux.second;
    return true;
}

void SkinnedZombies::updateInstances(const std::vector<mat4>& transforms,
                                     const std::vector<float>& hitFlashes,
                                     const std::vector<int>&   kinds) {
    if (!m_built) {
        return;
    }
    for (std::vector<float>& b : m_buckets) {
        b.clear();  /* conserve la capacité allouée */
    }

    const std::size_t n = std::min({transforms.size(), hitFlashes.size(), kinds.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const auto          kind = static_cast<unsigned int>(kinds[i]);
        std::vector<float>& dst  = m_buckets[bucketIndex(kinds[i])];
        if (dst.size() / FLOATS_PER_INSTANCE >= m_capacity) {
            continue;  /* filet de sécurité : tampon de la variante plein */
        }
        const float* src = glm::value_ptr(transforms[i]);
        dst.insert(dst.end(), src, src + 16);
        dst.push_back(hitFlashes[i]);
        /* Graine de couleur stable par zombie, tirée du même "kind" : décale la
           teinte de la tenue pour varier la horde (voir zombie_skinned.frag). */
        dst.push_back(static_cast<float>((kind * 2654435761u) % 1000u) / 1000.0f);
    }
}

void SkinnedZombies::draw(Shader& shader, float timeSeconds) {
    if (!m_built) {
        return;
    }
    if (m_model.texture() != nullptr) {
        m_model.texture()->bind(0);
    }

    const std::size_t variants = m_parts.size();
    const auto        G        = static_cast<std::size_t>(m_phaseGroups);
    for (std::size_t g = 0; g < G; ++g) {
        /* Un groupe n'est posé que s'il porte au moins une instance. */
        bool anyInGroup = false;
        for (std::size_t v = 0; v < variants && !anyInGroup; ++v) {
            anyInGroup = !m_buckets[v * G + g].empty();
        }
        if (!anyInGroup) {
            continue;
        }

        const float             tg      = timeSeconds + m_phaseOffset[g];
        const std::vector<mat4> globals = m_model.poseAtTime(tg);

        for (std::size_t v = 0; v < variants; ++v) {
            std::vector<float>& bucket = m_buckets[v * G + g];
            if (bucket.empty()) {
                continue;
            }
            const auto count = static_cast<GLsizei>(bucket.size() / FLOATS_PER_INSTANCE);

            const std::vector<mat4> bones = posedBones(v, globals, tg);
            /* Les lueurs d'yeux de ce lot suivent la pose qu'on s'apprête à
               dessiner (voir eyeAnchors) : elles se lisent après ce draw. */
            storeEyePoints(v * G + g, v, bones);
            shader.setMat4Array("u_bones", glm::value_ptr(bones[0]),
                                static_cast<int>(bones.size()));

            const Part& p = m_parts[v];
            glBindBuffer(GL_ARRAY_BUFFER, p.instanceVbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            static_cast<GLsizeiptr>(bucket.size() * sizeof(float)), bucket.data());
            glBindVertexArray(p.vao);
            glDrawElementsInstanced(GL_TRIANGLES, p.indexCount, GL_UNSIGNED_INT, nullptr, count);
        }
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} /* namespace artouste::render */
