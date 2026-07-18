/*
 * SkinnedModel.cpp
 * Voir SkinnedModel.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/SkinnedModel.hpp"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>

namespace artouste::render {

namespace {

/* Hauteur cible (m) d'un zombie une fois recalé, comme pour le modèle statique
   (render::combat::Zombies) : le pack est exporté en centimètres, pivot au sol
   mais centré arbitrairement en X/Z. */
constexpr float TARGET_HEIGHT_M = 1.80f;

mat4 toGlm(const aiMatrix4x4& a) {
    /* Assimp range par lignes, GLM par colonnes : la conversion transpose. */
    return mat4(a.a1, a.b1, a.c1, a.d1,   /* */
                a.a2, a.b2, a.c2, a.d2,   /* */
                a.a3, a.b3, a.c3, a.d3,   /* */
                a.a4, a.b4, a.c4, a.d4);
}

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
    const auto it = std::upper_bound(keys.begin(), keys.end(), t,
                                     [](float v, const std::pair<float, vec3>& k) { return v < k.first; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const float span = b.first - a.first;
    const float f    = span > 1e-8f ? (t - a.first) / span : 0.0f;
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
    const auto it = std::upper_bound(keys.begin(), keys.end(), t,
                                     [](float v, const std::pair<float, quat>& k) { return v < k.first; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    const float span = b.first - a.first;
    const float f    = span > 1e-8f ? (t - a.first) / span : 0.0f;
    return glm::slerp(a.second, b.second, f);
}

}  /* namespace */

SkinnedModel::SkinnedModel(const std::filesystem::path& path) {
    Assimp::Importer importer;
    /* LimitBoneWeights ramène chaque sommet à au plus 4 influences (notre
       format). Pas de PreTransformVertices : il détruirait os et animations. */
    const aiScene* scene = importer.ReadFile(
        path.string(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                           aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        scene->mRootNode == nullptr) {
        std::fprintf(stderr, "[SkinnedModel] échec %s : %s\n", path.string().c_str(),
                     importer.GetErrorString());
        return;
    }

    /* --- Arbre des noeuds : aplati en tableau, parent avant enfants --------- */
    std::unordered_map<std::string, int> nodeByName;
    /* Parcours en profondeur explicite (pile), en enregistrant chaque noeud à la
       suite : un parent reçoit toujours un index inférieur à ses enfants, ce qui
       permet plus tard de calculer les matrices globales en une seule passe. */
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
        /* Empilement en ordre inverse pour un parcours gauche-droite stable. */
        for (int c = static_cast<int>(p.node->mNumChildren) - 1; c >= 0; --c) {
            stack.push_back({p.node->mChildren[static_cast<unsigned int>(c)], index});
        }
    }
    m_globalInverse = glm::inverse(toGlm(scene->mRootNode->mTransformation));

    /* --- Animation : une seule "prise" couvrant tous les squelettes --------- */
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
            chan.posKeys.reserve(ch->mNumPositionKeys);
            for (unsigned int k = 0; k < ch->mNumPositionKeys; ++k) {
                const aiVectorKey& vk = ch->mPositionKeys[k];
                chan.posKeys.emplace_back(static_cast<float>(vk.mTime / tps),
                                          vec3(vk.mValue.x, vk.mValue.y, vk.mValue.z));
            }
            chan.rotKeys.reserve(ch->mNumRotationKeys);
            for (unsigned int k = 0; k < ch->mNumRotationKeys; ++k) {
                const aiQuatKey& qk = ch->mRotationKeys[k];
                chan.rotKeys.emplace_back(static_cast<float>(qk.mTime / tps),
                                          quat(qk.mValue.w, qk.mValue.x, qk.mValue.y, qk.mValue.z));
            }
            chan.scaleKeys.reserve(ch->mNumScalingKeys);
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

    /* --- Variantes de personnage : maillages skinnés ------------------------ */
    for (unsigned int mi = 0; mi < scene->mNumMeshes; ++mi) {
        const aiMesh* am = scene->mMeshes[mi];
        if (am->mNumBones == 0) {
            continue;  /* maillage non skinné (helper) : ignoré */
        }
        /* On ne garde que les CORPS de personnage (nom contenant "zombie") :
           le pack contient aussi des accessoires skinnés isolés (cheveux,
           bonnets, "acc_*") qui, tirés comme des variantes à part entière,
           apparaîtraient en touffes flottantes. Les zombies sont donc chauves,
           faute de rattacher les accessoires à un corps. */
        {
            std::string name = am->mName.C_Str();
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name.find("zombie") == std::string::npos) {
                continue;
            }
        }

        MeshData md;
        md.vertices.resize(am->mNumVertices);
        std::vector<int> influences(am->mNumVertices, 0);
        for (unsigned int v = 0; v < am->mNumVertices; ++v) {
            SkinnedVertex& sv = md.vertices[v];
            const aiVector3D& p = am->mVertices[v];
            sv.position         = vec3(p.x, p.y, p.z);
            if (am->mNormals != nullptr) {
                const aiVector3D& n = am->mNormals[v];
                sv.normal           = vec3(n.x, n.y, n.z);
            }
            if (am->HasTextureCoords(0)) {
                sv.uv = vec2(am->mTextureCoords[0][v].x, am->mTextureCoords[0][v].y);
            }
        }

        md.boneNode.resize(am->mNumBones);
        md.boneOffset.resize(am->mNumBones);
        for (unsigned int b = 0; b < am->mNumBones; ++b) {
            const aiBone* bone = am->mBones[b];
            const auto    it   = nodeByName.find(bone->mName.C_Str());
            md.boneNode[b]     = (it != nodeByName.end()) ? it->second : 0;
            md.boneOffset[b]   = toGlm(bone->mOffsetMatrix);
            for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                const aiVertexWeight& vw = bone->mWeights[w];
                int&                  cnt = influences[vw.mVertexId];
                if (cnt < 4) {
                    SkinnedVertex& sv = md.vertices[vw.mVertexId];
                    sv.joints[cnt]    = static_cast<int>(b);
                    sv.weights[cnt]   = vw.mWeight;
                    ++cnt;
                }
            }
        }
        /* Normalisation des poids (somme ~1) ; sommet sans influence rattaché à
           l'os 0, pour ne jamais s'effondrer à l'origine. */
        for (SkinnedVertex& sv : md.vertices) {
            const float sum = sv.weights[0] + sv.weights[1] + sv.weights[2] + sv.weights[3];
            if (sum > 1e-5f) {
                for (float& w : sv.weights) {
                    w /= sum;
                }
            } else {
                sv.weights[0] = 1.0f;
            }
        }

        md.indices.reserve(am->mNumFaces * 3);
        for (unsigned int f = 0; f < am->mNumFaces; ++f) {
            const aiFace& face = am->mFaces[f];
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                md.indices.push_back(face.mIndices[k]);
            }
        }

        /* localFix est calibré plus bas, sur la sortie RÉELLEMENT posée par le
           rig (voir la passe de calibration après la boucle) : le pack a déjà
           sa propre mise à l'échelle et son orientation, calibrer sur les
           sommets bruts double l'échelle et ignore la rotation du squelette. */
        if (!md.indices.empty() && md.boneNode.size() <= static_cast<std::size_t>(MAX_BONES)) {
            m_meshes.push_back(std::move(md));
        }
    }

    /* --- Calibration de localFix, par variante, sur la pose RÉELLE ----------
     * Le rig applique déjà sa propre échelle (cm -> m) et une orientation Z-up,
     * et chaque variante est posée à un endroit différent de la scène du pack.
     * On mesure donc la sortie effectivement posée (os sans localFix) à t=0,
     * puis on construit localFix pour : remettre l'axe vertical sur Y (rotation
     * Z-up -> Y-up), recentrer en X/Z, poser les pieds à Y=0 et mettre à la
     * taille cible. Calibrer sur les sommets bruts (comme le modèle statique)
     * doublait l'échelle et ignorait la rotation, d'où des zombies invisibles. */
    {
        const std::vector<mat4> globals0 = poseAtTime(0.0f);
        /* Z-up (rig) -> Y-up (monde) : +Z devient +Y. */
        const mat4 zUpToYUp = glm::rotate(mat4(1.0f), -HALF_PI, vec3{1.0f, 0.0f, 0.0f});
        for (MeshData& md : m_meshes) {
            std::vector<mat4> bones(md.boneNode.size());
            for (std::size_t b = 0; b < md.boneNode.size(); ++b) {
                bones[b] = zUpToYUp * m_globalInverse *
                           globals0[static_cast<std::size_t>(md.boneNode[b])] * md.boneOffset[b];
            }
            vec3 lo{std::numeric_limits<float>::max()};
            vec3 hi{std::numeric_limits<float>::lowest()};
            for (const SkinnedVertex& sv : md.vertices) {
                mat4 skin(0.0f);
                for (int k = 0; k < 4; ++k) {
                    skin += sv.weights[k] * bones[static_cast<std::size_t>(sv.joints[k])];
                }
                const vec3 p = vec3(skin * vec4(sv.position, 1.0f));
                lo = glm::min(lo, p);
                hi = glm::max(hi, p);
            }
            const float height = hi.y - lo.y;
            const float scale  = (height > 1e-4f) ? TARGET_HEIGHT_M / height : 1.0f;
            const vec3  center{(lo.x + hi.x) * 0.5f, lo.y, (lo.z + hi.z) * 0.5f};
            /* localFix appliqué à GAUCHE des os (voir boneMatrices) : d'abord la
               rotation Z-up -> Y-up, puis recentrage/pieds au sol, puis échelle. */
            md.localFix = glm::scale(mat4(1.0f), vec3{scale}) *
                          glm::translate(mat4(1.0f), -center) * zUpToYUp;
        }
    }

    /* --- Table de dérive (root motion) par variante ------------------------
     * On mesure le centre horizontal (X,Z final) de chaque variante a plusieurs
     * instants : l'animation "Take 001" déplace le personnage (~0,4 m), ce qui,
     * non compensé, le fait GLISSER par rapport a sa position logique (et
     * différemment selon le groupe de phase). Le rendu retranche cette dérive
     * pour l'épingler au sol. Dernier échantillon = t bouclé sur le premier. */
    m_centerTable.resize(m_meshes.size());
    for (std::size_t mi = 0; mi < m_meshes.size(); ++mi) {
        m_centerTable[mi].resize(static_cast<std::size_t>(CENTER_SAMPLES));
        for (int s = 0; s < CENTER_SAMPLES; ++s) {
            const float t = (m_durationS > 1e-4f)
                                ? m_durationS * static_cast<float>(s) / static_cast<float>(CENTER_SAMPLES - 1)
                                : 0.0f;
            const std::vector<mat4> globals = poseAtTime(t);
            const std::vector<mat4> bones   = boneMatrices(mi, globals);
            const MeshData&         md      = m_meshes[mi];
            vec3 lo{std::numeric_limits<float>::max()};
            vec3 hi{std::numeric_limits<float>::lowest()};
            for (const SkinnedVertex& sv : md.vertices) {
                mat4 skin(0.0f);
                for (int k = 0; k < 4; ++k) {
                    skin += sv.weights[k] * bones[static_cast<std::size_t>(sv.joints[k])];
                }
                const vec3 p = vec3(skin * vec4(sv.position, 1.0f));
                lo = glm::min(lo, p);
                hi = glm::max(hi, p);
            }
            m_centerTable[mi][static_cast<std::size_t>(s)] =
                vec2{(lo.x + hi.x) * 0.5f, (lo.z + hi.z) * 0.5f};
        }
    }

    /* --- Texture atlas embarquée (unique, partagée par toutes les variantes) - */
    if (scene->mNumTextures > 0) {
        const aiTexture* at = scene->mTextures[0];
        if (at->mHeight == 0) {  /* image compressée (PNG/JPG) : mWidth = taille en octets */
            m_texture = std::make_unique<Texture>(reinterpret_cast<const unsigned char*>(at->pcData),
                                                  static_cast<std::size_t>(at->mWidth));
        }
    }

    m_built = !m_meshes.empty();
    if (m_built) {
        std::printf("[SkinnedModel] %s : %zu variantes, %zu noeuds, %.2f s d'animation.\n",
                    path.filename().string().c_str(), m_meshes.size(), m_nodes.size(),
                    static_cast<double>(m_durationS));
    } else {
        std::fprintf(stderr, "[SkinnedModel] %s : aucune variante skinnée exploitable.\n",
                     path.string().c_str());
    }
}

std::vector<mat4> SkinnedModel::poseAtTime(float t) const {
    std::vector<mat4> globals(m_nodes.size(), mat4(1.0f));
    const float       tt = (m_durationS > 1e-4f) ? std::fmod(std::fmod(t, m_durationS) + m_durationS,
                                                             m_durationS)
                                                 : 0.0f;
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

std::vector<mat4> SkinnedModel::boneMatrices(std::size_t meshIndex,
                                             const std::vector<mat4>& globals) const {
    const MeshData&   md = m_meshes[meshIndex];
    std::vector<mat4> out(md.boneNode.size());
    for (std::size_t b = 0; b < md.boneNode.size(); ++b) {
        out[b] = md.localFix * m_globalInverse *
                 globals[static_cast<std::size_t>(md.boneNode[b])] * md.boneOffset[b];
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
    const float phase = (m_durationS > 1e-4f) ? tt / m_durationS : 0.0f;  /* 0..1 */
    const float f     = phase * static_cast<float>(CENTER_SAMPLES - 1);
    const int   i0    = static_cast<int>(f);
    const int   i1    = std::min(i0 + 1, CENTER_SAMPLES - 1);
    const float frac  = f - static_cast<float>(i0);
    return table[static_cast<std::size_t>(i0)] * (1.0f - frac) +
           table[static_cast<std::size_t>(i1)] * frac;
}

}  /* namespace artouste::render */
