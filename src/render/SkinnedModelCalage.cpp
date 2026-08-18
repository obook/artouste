/*
 * SkinnedModelCalage.cpp
 * Calage d'une variante de personnage sur sa pose réelle, et ancrage des
 * lueurs d'yeux sur l'os de tête.
 *
 * Le rig applique sa propre échelle et une orientation Z-up, et chaque
 * variante est posée ailleurs dans la scène du pack : on mesure la sortie
 * effectivement posée plutôt que de croire le fichier.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/SkinnedModel.hpp"

#include "render/SkinnedModelReglages.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace artouste::render {

void SkinnedModel::calibrerVariantes() {
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
}

void SkinnedModel::ancrerYeux() {
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
}

} /* namespace artouste::render */
