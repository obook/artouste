/*
 * ExplosionModelCles.hpp
 * Lecture d'une piste d'animation entre deux clés.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <utility>
#include <vector>

namespace artouste::render {

inline vec3 sampleVec(const std::vector<std::pair<float, vec3>>& keys, float t, const vec3& fallback) {
    if (keys.empty()) {
        return fallback;
    }
    if (t <= keys.front().first) {
        return keys.front().second;
    }
    if (t >= keys.back().first) {
        return keys.back().second;
    }
    const auto it = std::upper_bound(keys.begin(), keys.end(), t,
                                     [](float v, const std::pair<float, vec3>& k) { return v < k.first; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const float span = b.first - a.first;
    const float f    = span > 1e-8f ? (t - a.first) / span : 0.0f;
    return a.second + (b.second - a.second) * f;
}

inline quat sampleQuat(const std::vector<std::pair<float, quat>>& keys, float t) {
    if (keys.empty()) {
        return quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    if (t <= keys.front().first) {
        return keys.front().second;
    }
    if (t >= keys.back().first) {
        return keys.back().second;
    }
    const auto it = std::upper_bound(keys.begin(), keys.end(), t,
                                     [](float v, const std::pair<float, quat>& k) { return v < k.first; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const float span = b.first - a.first;
    const float f    = span > 1e-8f ? (t - a.first) / span : 0.0f;
    return glm::slerp(a.second, b.second, f);
}

} /* namespace artouste::render */
