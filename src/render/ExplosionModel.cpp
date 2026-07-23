/*
 * ExplosionModel.cpp
 * Voir ExplosionModel.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/ExplosionModel.hpp"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>

namespace artouste::render {

namespace {

mat4 toGlm(const aiMatrix4x4& a) {
    return mat4(a.a1, a.b1, a.c1, a.d1,   /* */
                a.a2, a.b2, a.c2, a.d2,   /* */
                a.a3, a.b3, a.c3, a.d3,   /* */
                a.a4, a.b4, a.c4, a.d4);
}

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
    const auto it = std::upper_bound(keys.begin(), keys.end(), t,
                                     [](float v, const std::pair<float, vec3>& k) { return v < k.first; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const float span = b.first - a.first;
    const float f    = span > 1e-8f ? (t - a.first) / span : 0.0f;
    return a.second + (b.second - a.second) * f;
}

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
    const auto it = std::upper_bound(keys.begin(), keys.end(), t,
                                     [](float v, const std::pair<float, quat>& k) { return v < k.first; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const float span = b.first - a.first;
    const float f    = span > 1e-8f ? (t - a.first) / span : 0.0f;
    return glm::slerp(a.second, b.second, f);
}

}  /* namespace */

ExplosionModel::ExplosionModel(const std::filesystem::path& path) {
    Assimp::Importer importer;
    const aiScene*   scene = importer.ReadFile(
        path.string(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                           aiProcess_GenSmoothNormals);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        scene->mRootNode == nullptr) {
        std::fprintf(stderr, "[ExplosionModel] échec %s : %s\n", path.string().c_str(),
                     importer.GetErrorString());
        return;
    }

    /* --- Arbre des nœuds (parent avant enfants) + maillages par nœud --------- */
    std::unordered_map<std::string, int>          nodeByName;
    std::vector<std::pair<int, const aiMesh*>>    meshNodePairs;
    struct Pending {
        const aiNode* node;
        int           parent;
    };
    std::vector<Pending> stack{{scene->mRootNode, -1}};
    while (!stack.empty()) {
        const Pending p = stack.back();
        stack.pop_back();
        const int index = static_cast<int>(m_nodes.size());
        Node      n;
        n.localDefault = toGlm(p.node->mTransformation);
        n.parent       = p.parent;
        m_nodes.push_back(n);
        nodeByName[p.node->mName.C_Str()] = index;
        for (unsigned int i = 0; i < p.node->mNumMeshes; ++i) {
            meshNodePairs.emplace_back(index, scene->mMeshes[p.node->mMeshes[i]]);
        }
        for (int c = static_cast<int>(p.node->mNumChildren) - 1; c >= 0; --c) {
            stack.push_back({p.node->mChildren[static_cast<unsigned int>(c)], index});
        }
    }

    /* --- Animation (une prise, ici de l'échelle par nœud) -------------------- */
    if (scene->mNumAnimations > 0) {
        const aiAnimation* anim = scene->mAnimations[0];
        const double       tps  = anim->mTicksPerSecond > 1e-6 ? anim->mTicksPerSecond : 25.0;
        m_durationS             = static_cast<float>(anim->mDuration / tps);
        for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
            const aiNodeAnim* ch = anim->mChannels[c];
            const auto        it = nodeByName.find(ch->mNodeName.C_Str());
            if (it == nodeByName.end()) {
                continue;
            }
            Channel chan;
            for (unsigned int k = 0; k < ch->mNumPositionKeys; ++k) {
                const aiVectorKey& vk = ch->mPositionKeys[k];
                chan.posKeys.emplace_back(static_cast<float>(vk.mTime / tps),
                                          vec3(vk.mValue.x, vk.mValue.y, vk.mValue.z));
            }
            for (unsigned int k = 0; k < ch->mNumRotationKeys; ++k) {
                const aiQuatKey& qk = ch->mRotationKeys[k];
                chan.rotKeys.emplace_back(static_cast<float>(qk.mTime / tps),
                                          quat(qk.mValue.w, qk.mValue.x, qk.mValue.y, qk.mValue.z));
            }
            for (unsigned int k = 0; k < ch->mNumScalingKeys; ++k) {
                const aiVectorKey& sk = ch->mScalingKeys[k];
                chan.scaleKeys.emplace_back(static_cast<float>(sk.mTime / tps),
                                            vec3(sk.mValue.x, sk.mValue.y, sk.mValue.z));
            }
            m_nodes[static_cast<std::size_t>(it->second)].channel =
                static_cast<int>(m_channels.size());
            m_channels.push_back(std::move(chan));
        }
    }

    /* --- Maillages : géométrie statique + nœud porteur ----------------------- */
    std::vector<std::vector<Vertex>> partVerts;  /* copies CPU, pour la bbox de localFix */
    std::vector<int>                 partNode;
    for (const auto& mn : meshNodePairs) {
        const aiMesh* am = mn.second;
        if (am->mNumVertices == 0 || am->mNumFaces == 0) {
            continue;
        }
        std::vector<Vertex> vertices(am->mNumVertices);
        for (unsigned int v = 0; v < am->mNumVertices; ++v) {
            Vertex& vert         = vertices[v];
            const aiVector3D& pp = am->mVertices[v];
            vert.position        = vec3(pp.x, pp.y, pp.z);
            if (am->mNormals != nullptr) {
                const aiVector3D& nn = am->mNormals[v];
                vert.normal          = vec3(nn.x, nn.y, nn.z);
            }
            if (am->HasTextureCoords(0)) {
                vert.uv = vec2(am->mTextureCoords[0][v].x, am->mTextureCoords[0][v].y);
            }
            vert.color = vec3(1.0f);
        }
        std::vector<unsigned int> indices;
        indices.reserve(am->mNumFaces * 3);
        for (unsigned int f = 0; f < am->mNumFaces; ++f) {
            const aiFace& face = am->mFaces[f];
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                indices.push_back(face.mIndices[k]);
            }
        }
        m_parts.push_back(MeshPart{Mesh(vertices, indices), mn.first});
        partVerts.push_back(std::move(vertices));
        partNode.push_back(mn.first);
    }

    /* --- localFix : normalise l'explosion à un rayon unité ------------------
     * On échantillonne l'animation et on prend l'étendue maximale des maillages
     * posés (transformation globale de leur nœud, qui inclut l'échelle du
     * flipbook). localFix recentre et met à l'échelle pour que le plus grand
     * demi-côté vaille 1 ; le renderer applique ensuite le rayon monde voulu. */
    if (!m_parts.empty()) {
        vec3        gmin{std::numeric_limits<float>::max()};
        vec3        gmax{std::numeric_limits<float>::lowest()};
        const int   SAMPLES = 16;
        const float dur     = (m_durationS > 1e-4f) ? m_durationS : 1.0f;
        for (int s = 0; s < SAMPLES; ++s) {
            const float             t       = dur * static_cast<float>(s) / static_cast<float>(SAMPLES - 1);
            const std::vector<mat4> globals = poseAtTime(t);
            for (std::size_t pi = 0; pi < partVerts.size(); ++pi) {
                const mat4& g = globals[static_cast<std::size_t>(partNode[pi])];
                for (const Vertex& vv : partVerts[pi]) {
                    const vec3 p = vec3(g * vec4(vv.position, 1.0f));
                    gmin = glm::min(gmin, p);
                    gmax = glm::max(gmax, p);
                }
            }
        }
        const vec3  center = (gmin + gmax) * 0.5f;
        const vec3  half   = (gmax - gmin) * 0.5f;
        const float maxHalf = std::max(half.x, std::max(half.y, half.z));
        const float scale   = (maxHalf > 1e-4f) ? 1.0f / maxHalf : 1.0f;
        m_localFix = glm::scale(mat4(1.0f), vec3{scale}) * glm::translate(mat4(1.0f), -center);
    }

    /* --- Texture atlas embarquée ------------------------------------------- */
    if (scene->mNumTextures > 0) {
        const aiTexture* at = scene->mTextures[0];
        if (at->mHeight == 0) {
            m_texture = std::make_unique<Texture>(reinterpret_cast<const unsigned char*>(at->pcData),
                                                  static_cast<std::size_t>(at->mWidth));
        }
    }

    m_built = !m_parts.empty();
    if (m_built) {
        std::printf("[ExplosionModel] %s : %zu images, %zu nœuds, %.2f s.\n",
                    path.filename().string().c_str(), m_parts.size(), m_nodes.size(),
                    static_cast<double>(m_durationS));
    } else {
        std::fprintf(stderr, "[ExplosionModel] %s : aucun maillage exploitable.\n",
                     path.string().c_str());
    }
}

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

}  /* namespace artouste::render */
