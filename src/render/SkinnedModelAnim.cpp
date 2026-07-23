/*
 * SkinnedModelAnim.cpp
 * Évaluation de l'animation d'un modèle skinné déjà chargé : pose du
 * squelette à un instant donné (poseAtTime), matrices d'os finales
 * (boneMatrices) et dérive de root motion (rootDriftXZ). Le chargement
 * (import Assimp, calibration) est dans SkinnedModelLoad.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/SkinnedModel.hpp"

#include <algorithm>
#include <cmath>

namespace artouste::render {

namespace {

/* Interpole une piste de clés (translation/échelle) à l'instant t (secondes).
   Clés triées par temps ; recherche dichotomique du segment encadrant. */
vec3 sampleVec(const std::vector<std::pair<float, vec3>>& keys, float t, const vec3& fallback) {
    if (keys.empty()) {
        return fallback;
    }
    if (t <= keys.front().first) {
        return keys.front().second;
    }
    if (t >= keys.back().first) {
        return keys.back().second;
    }
    const auto it =
        std::upper_bound(keys.begin(), keys.end(), t, [](float v, const std::pair<float, vec3>& k) {
            return v < k.first;
        });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const float span = b.first - a.first;
    const float f = span > 1e-8f ? (t - a.first) / span : 0.0f;
    return a.second + (b.second - a.second) * f;
}

/* Idem pour une piste de rotations : interpolation sphérique (slerp). */
quat sampleQuat(const std::vector<std::pair<float, quat>>& keys, float t) {
    if (keys.empty()) {
        return quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    if (t <= keys.front().first) {
        return keys.front().second;
    }
    if (t >= keys.back().first) {
        return keys.back().second;
    }
    const auto it =
        std::upper_bound(keys.begin(), keys.end(), t, [](float v, const std::pair<float, quat>& k) {
            return v < k.first;
        });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const float span = b.first - a.first;
    const float f = span > 1e-8f ? (t - a.first) / span : 0.0f;
    return glm::slerp(a.second, b.second, f);
}

} /* namespace */

std::vector<mat4> SkinnedModel::poseAtTime(float t) const {
    std::vector<mat4> globals(m_nodes.size(), mat4(1.0f));
    const float tt = (m_durationS > 1e-4f)
                         ? std::fmod(std::fmod(t, m_durationS) + m_durationS, m_durationS)
                         : 0.0f;
    for (std::size_t i = 0; i < m_nodes.size(); ++i) {
        const Node& n = m_nodes[i];
        mat4 local;
        if (n.channel >= 0) {
            const Channel& ch = m_channels[static_cast<std::size_t>(n.channel)];
            const vec3 tr = sampleVec(ch.posKeys, tt, vec3{0.0f});
            const quat rot = sampleQuat(ch.rotKeys, tt);
            const vec3 sc = sampleVec(ch.scaleKeys, tt, vec3{1.0f});
            local = glm::translate(mat4(1.0f), tr) * mat4_cast(rot) * glm::scale(mat4(1.0f), sc);
        } else {
            local = n.localDefault;
        }
        globals[i] = (n.parent >= 0) ? globals[static_cast<std::size_t>(n.parent)] * local : local;
    }
    return globals;
}

std::vector<mat4> SkinnedModel::boneMatrices(std::size_t meshIndex,
                                             const std::vector<mat4>& globals) const {
    const MeshData& md = m_meshes[meshIndex];
    std::vector<mat4> out(md.boneNode.size());
    for (std::size_t b = 0; b < md.boneNode.size(); ++b) {
        out[b] = md.localFix * m_globalInverse * globals[static_cast<std::size_t>(md.boneNode[b])] *
                 md.boneOffset[b];
    }
    return out;
}

vec2 SkinnedModel::rootDriftXZ(std::size_t meshIndex, float t) const {
    if (meshIndex >= m_centerTable.size() || m_centerTable[meshIndex].empty()) {
        return vec2{0.0f};
    }
    const std::vector<vec2>& table = m_centerTable[meshIndex];
    const float tt = (m_durationS > 1e-4f)
                         ? std::fmod(std::fmod(t, m_durationS) + m_durationS, m_durationS)
                         : 0.0f;
    const float phase = (m_durationS > 1e-4f) ? tt / m_durationS : 0.0f; /* 0..1 */
    const float f = phase * static_cast<float>(CENTER_SAMPLES - 1);
    const int i0 = static_cast<int>(f);
    const int i1 = std::min(i0 + 1, CENTER_SAMPLES - 1);
    const float frac = f - static_cast<float>(i0);
    return table[static_cast<std::size_t>(i0)] * (1.0f - frac) +
           table[static_cast<std::size_t>(i1)] * frac;
}

} /* namespace artouste::render */
