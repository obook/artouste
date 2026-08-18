/*
 * SkinnedModelLoad.cpp
 * Chargement du modèle skinné : import Assimp (squelette, poids de sommets,
 * canaux d'animation), sélection des variantes de personnage exploitables et
 * calibration de leur correction de recentrage/échelle (localFix), plus la
 * table de dérive (root motion). L'évaluation de l'animation (poseAtTime,
 * boneMatrices, rootDriftXZ) est dans SkinnedModelAnim.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/SkinnedModel.hpp"

#include "render/SkinnedModelReglages.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>

namespace artouste::render {



SkinnedModel::SkinnedModel(const std::filesystem::path& path) {
    Assimp::Importer importer;
    /* LimitBoneWeights ramène chaque sommet à au plus 4 influences (notre
       format). Pas de PreTransformVertices : il détruirait os et animations. */
    const aiScene* scene =
        importer.ReadFile(path.string(),
                          aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                              aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        scene->mRootNode == nullptr) {
        std::fprintf(stderr,
                     "[SkinnedModel] échec %s : %s\n",
                     path.string().c_str(),
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
        int parent;
    };
    std::vector<Pending> stack{{scene->mRootNode, -1}};
    while (!stack.empty()) {
        const Pending p = stack.back();
        stack.pop_back();
        const int index = static_cast<int>(m_nodes.size());
        Node n;
        n.localDefault = toGlm(p.node->mTransformation);
        n.parent = p.parent;
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
        const double tps = anim->mTicksPerSecond > 1e-6 ? anim->mTicksPerSecond : 25.0;
        m_durationS = static_cast<float>(anim->mDuration / tps);
        for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
            const aiNodeAnim* ch = anim->mChannels[c];
            const auto it = nodeByName.find(ch->mNodeName.C_Str());
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
            continue; /* maillage non skinné (helper) : ignoré */
        }
        /* On ne garde que les CORPS de personnage (nom contenant "zombie") :
           le pack contient aussi des accessoires skinnés isolés (cheveux,
           bonnets, "acc_*") qui, tirés comme des variantes à part entière,
           apparaîtraient en touffes flottantes. Les zombies sont donc chauves,
           faute de rattacher les accessoires à un corps. */
        {
            std::string name = am->mName.C_Str();
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (name.find("zombie") == std::string::npos) {
                continue;
            }
            /* lpFemale_zombie_C reste figée en pose de repos (jambes, bras) alors
               que ses données d'animation sont, à l'examen, aussi complètes que
               les 9 autres variantes (canaux, poids de sommets, squelette --
               vérifié os par os) : le défaut vient très probablement de la
               conversion Assimp de ce maillage précis (glTF -> aiMesh/aiBone),
               pas du fichier source. Faute d'avoir isolé la cause exacte,
               exclue explicitement plutôt que de faire glisser un zombie sur
               dix devant le joueur. */
            if (name.find("lpfemale_zombie_c") != std::string::npos) {
                continue;
            }
        }

        MeshData md;
        md.vertices.resize(am->mNumVertices);
        std::vector<int> influences(am->mNumVertices, 0);
        for (unsigned int v = 0; v < am->mNumVertices; ++v) {
            SkinnedVertex& sv = md.vertices[v];
            const aiVector3D& p = am->mVertices[v];
            sv.position = vec3(p.x, p.y, p.z);
            if (am->mNormals != nullptr) {
                const aiVector3D& n = am->mNormals[v];
                sv.normal = vec3(n.x, n.y, n.z);
            }
            if (am->HasTextureCoords(0)) {
                sv.uv = vec2(am->mTextureCoords[0][v].x, am->mTextureCoords[0][v].y);
            }
        }

        md.boneNode.resize(am->mNumBones);
        md.boneOffset.resize(am->mNumBones);
        for (unsigned int b = 0; b < am->mNumBones; ++b) {
            const aiBone* bone = am->mBones[b];
            const auto it = nodeByName.find(bone->mName.C_Str());
            md.boneNode[b] = (it != nodeByName.end()) ? it->second : 0;
            md.boneOffset[b] = toGlm(bone->mOffsetMatrix);
            for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                const aiVertexWeight& vw = bone->mWeights[w];
                int& cnt = influences[vw.mVertexId];
                if (cnt < 4) {
                    SkinnedVertex& sv = md.vertices[vw.mVertexId];
                    sv.joints[cnt] = static_cast<int>(b);
                    sv.weights[cnt] = vw.mWeight;
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

    calibrerVariantes();

    ancrerYeux();

    /* --- Texture atlas embarquée (unique, partagée par toutes les variantes) - */
    if (scene->mNumTextures > 0) {
        const aiTexture* at = scene->mTextures[0];
        if (at->mHeight == 0) { /* image compressée (PNG/JPG) : mWidth = taille en octets */
            m_texture =
                std::make_unique<Texture>(reinterpret_cast<const unsigned char*>(at->pcData),
                                          static_cast<std::size_t>(at->mWidth));
        }
    }

    m_built = !m_meshes.empty();
    if (m_built) {
        std::printf("[SkinnedModel] %s : %zu variantes, %zu noeuds, %.2f s d'animation.\n",
                    path.filename().string().c_str(),
                    m_meshes.size(),
                    m_nodes.size(),
                    static_cast<double>(m_durationS));
    } else {
        std::fprintf(stderr,
                     "[SkinnedModel] %s : aucune variante skinnée exploitable.\n",
                     path.string().c_str());
    }
}

} /* namespace artouste::render */
