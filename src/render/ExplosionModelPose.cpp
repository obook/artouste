/*
 * ExplosionModelPose.cpp
 * Pose de l'explosion à un instant donné, interpolée entre ses clés.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/ExplosionModel.hpp"

#include "render/ExplosionModelCles.hpp"

#include <algorithm>

namespace artouste::render {

std::vector<mat4> ExplosionModel::poseAtTime(float t) const {
    std::vector<mat4> globals(m_nodes.size(), mat4(1.0f));
    const float       tt = (m_durationS > 1e-4f) ? clamp(t, 0.0f, m_durationS) : 0.0f;
    for (std::size_t i = 0; i < m_nodes.size(); ++i) {
        const Node& n = m_nodes[i];
        mat4        local;
        if (n.channel >= 0) {
            const Channel& ch = m_channels[static_cast<std::size_t>(n.channel)];
            const vec3     tr = sampleVec(ch.posKeys, tt, vec3{0.0f});
            const quat     rot = sampleQuat(ch.rotKeys, tt);
            const vec3     sc = sampleVec(ch.scaleKeys, tt, vec3{1.0f});
            local = glm::translate(mat4(1.0f), tr) * mat4_cast(rot) * glm::scale(mat4(1.0f), sc);
        } else {
            local = n.localDefault;
        }
        globals[i] = (n.parent >= 0) ? globals[static_cast<std::size_t>(n.parent)] * local : local;
    }
    return globals;
}

} /* namespace artouste::render */
