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

namespace {

/* Hauteur cible (m) d'un zombie une fois recalé, comme pour le modèle statique
   (render::combat::Zombies) : le pack est exporté en centimètres, pivot au sol
   mais centré arbitrairement en X/Z. */
constexpr float TARGET_HEIGHT_M = 1.80f;

/* Calibration de l'ancrage des yeux (voir la passe dédiée du constructeur).
   HEAD_SLICE_M est la tranche haute où l'on cherche l'os de tête ; le reste
   place le regard par rapport au NEZ, c'est-à-dire au point le plus avancé du
   crâne. Se caler sur la boîte du crâne ne suffisait pas : elle fait 18 à 29 cm
   de large selon la variante (oreilles et cheveux compris), si bien qu'un
   demi-écart pris sur elle posait les lueurs sur les oreilles. L'écart entre
   pupilles, lui, ne dépend pas de la coiffure : 6,4 cm chez l'humain. */
constexpr float HEAD_SLICE_M     = 0.12f;
constexpr float EYE_SPACING_M    = 0.032f;  /* demi-écart entre les deux yeux */
constexpr float EYE_ABOVE_NOSE_M = 0.015f;  /* les yeux sont juste au-dessus du nez */
constexpr float EYE_NOSE_INSET_M = 0.015f;  /* et en retrait de sa pointe */
/* Bornes verticales, comptées sous le sommet du crâne. Le point le plus avancé
   d'une tête n'est pas toujours le nez : sur trois des neuf variantes du pack,
   c'est le front ou une mèche, et le regard remontait alors sur le front (défaut
   signalé en jeu). On retient donc la PLUS BASSE des deux estimations, celle
   tirée du nez et celle tirée de la taille de la tête, avant de borner par le
   bas : trop haut se voit tout de suite, un peu bas passe pour un regard. */
constexpr float EYE_BELOW_CROWN_MIN_M = 0.11f;
constexpr float EYE_BELOW_CROWN_MAX_M = 0.18f;

mat4 toGlm(const aiMatrix4x4& a) {
    /* Assimp range par lignes, GLM par colonnes : la conversion transpose. */
    return mat4(a.a1,
                a.b1,
                a.c1,
                a.d1, /* */
                a.a2,
                a.b2,
                a.c2,
                a.d2, /* */
                a.a3,
                a.b3,
                a.c3,
                a.d3, /* */
                a.a4,
                a.b4,
                a.c4,
                a.d4);
}

} /* namespace */

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
            const float scale = (height > 1e-4f) ? TARGET_HEIGHT_M / height : 1.0f;
            const vec3 center{(lo.x + hi.x) * 0.5f, lo.y, (lo.z + hi.z) * 0.5f};
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
            const float t = (m_durationS > 1e-4f) ? m_durationS * static_cast<float>(s) /
                                                        static_cast<float>(CENTER_SAMPLES - 1)
                                                  : 0.0f;
            const std::vector<mat4> globals = poseAtTime(t);
            const std::vector<mat4> bones = boneMatrices(mi, globals);
            const MeshData& md = m_meshes[mi];
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

    /* --- Ancrage des lueurs d'yeux sur l'os de tête ------------------------
     * Les yeux étaient posés à un point fixe du repère du modèle (1,62 m de
     * haut, 11 cm en avant). Mesuré sur ce pack, le crâne s'en écarte de 13 à
     * 34 cm selon la variante et l'instant du cycle, root motion déjà compensé
     * -- plus qu'un crâne ne mesure : les lueurs flottaient à côté du visage.
     * On repère donc, par variante, l'os qui pilote le haut de la tête, et on
     * exprime les deux yeux DANS son repère : ils suivent ensuite la tête quoi
     * qu'elle fasse, sans rien coûter au rendu (deux produits matrice-point par
     * lot déjà posé).
     */
    {
        const std::vector<mat4> globals0 = poseAtTime(0.0f);
        for (std::size_t mi = 0; mi < m_meshes.size(); ++mi) {
            MeshData&               md    = m_meshes[mi];
            const std::vector<mat4> bones = boneMatrices(mi, globals0);

            /* Sommets posés à t=0, et l'os qui pèse le plus sur chacun. */
            std::vector<vec3> posed(md.vertices.size());
            std::vector<int>  dominant(md.vertices.size(), 0);
            float             top = std::numeric_limits<float>::lowest();
            for (std::size_t i = 0; i < md.vertices.size(); ++i) {
                const SkinnedVertex& sv = md.vertices[i];
                mat4                 skin(0.0f);
                int                  best = 0;
                for (int k = 0; k < 4; ++k) {
                    skin += sv.weights[k] * bones[static_cast<std::size_t>(sv.joints[k])];
                    if (sv.weights[k] > sv.weights[best]) {
                        best = k;
                    }
                }
                posed[i]    = vec3(skin * vec4(sv.position, 1.0f));
                dominant[i] = sv.joints[best];
                top         = std::max(top, posed[i].y);
            }

            /* Os de tête : celui qui domine le plus de sommets dans la tranche
               haute du personnage (le crâne, cheveux compris). */
            std::vector<int> votes(md.boneNode.size(), 0);
            for (std::size_t i = 0; i < posed.size(); ++i) {
                if (posed[i].y > top - HEAD_SLICE_M) {
                    ++votes[static_cast<std::size_t>(dominant[i])];
                }
            }
            const auto bestBone = std::max_element(votes.begin(), votes.end());
            if (bestBone == votes.end() || *bestBone == 0) {
                continue;  /* variante sans tête identifiable : pas de lueurs */
            }
            const auto eyeBone = static_cast<int>(std::distance(votes.begin(), bestBone));

            /* Sommet du crâne, et point le plus avancé des sommets pilotés par
               cet os : le nez. Le modèle regarde vers +Z, sens dans lequel le
               rendu oriente la marche (voir app::ZombieHorde::instanceMatrix),
               donc "le plus avancé" veut dire "de z maximal". Le nez donne les
               trois coordonnées utiles : l'axe du visage en X (une boîte de
               crâne est décentrée par la coiffure), la hauteur du regard juste
               au-dessus, et l'avancée du visage en Z. */
            float crown = std::numeric_limits<float>::lowest();
            vec3  nose{0.0f};
            bool  hasNose = false;
            for (std::size_t i = 0; i < posed.size(); ++i) {
                if (dominant[i] != eyeBone) {
                    continue;
                }
                crown = std::max(crown, posed[i].y);
                if (!hasNose || posed[i].z > nose.z) {
                    nose    = posed[i];
                    hasNose = true;
                }
            }
            if (!hasNose) {
                continue;
            }
            const float y = std::max(std::min(nose.y + EYE_ABOVE_NOSE_M,
                                              crown - EYE_BELOW_CROWN_MIN_M),
                                     crown - EYE_BELOW_CROWN_MAX_M);
            const float cx = nose.x;
            const float z  = nose.z - EYE_NOSE_INSET_M;

            const mat4 toBone = glm::inverse(bones[static_cast<std::size_t>(eyeBone)]);
            md.eyeBone        = eyeBone;
            md.eyeLocal[0]    = vec3(toBone * vec4(cx - EYE_SPACING_M, y, z, 1.0f));
            md.eyeLocal[1]    = vec3(toBone * vec4(cx + EYE_SPACING_M, y, z, 1.0f));
        }
    }

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
