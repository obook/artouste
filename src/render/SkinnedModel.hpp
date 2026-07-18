/*
 * SkinnedModel.hpp
 * Modèle 3D "skinné" (animation squelettique) chargé depuis un glTF/.glb via
 * Assimp, distinct du render::Model statique (qui aplatit les transformations
 * de noeuds dans les sommets et ne connaît ni os ni animation). Pensé pour le
 * mode zombie : le pack fourni contient plusieurs variantes de personnages
 * (chacune sa géométrie et son squelette) et une animation de marche commune.
 *
 * Le modèle expose, par variante :
 *   - la géométrie skinnée (position/normale/uv + 4 os et 4 poids par sommet) ;
 *   - la liste de ses os (noeud du squelette + matrice de bind inverse) ;
 *   - une correction de recentrage/échelle (localFix), le pack étant exporté en
 *     centimètres et pivot arbitraire.
 * Et, pour l'animation :
 *   - poseAtTime(t) pose TOUT le squelette à l'instant t (bouclé) et renvoie la
 *     matrice globale de chaque noeud ;
 *   - boneMatrices(variante, poses) en déduit les matrices d'os finales, prêtes
 *     pour l'uniforme u_bones du shader de skinning.
 * Le calcul CPU est volontairement séparé du rendu : render::combat::Zombies
 * pose le squelette une fois par groupe de phase, puis dessine chaque variante
 * en instancié (voir l'approche par groupes de phase du mode zombie).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Texture.hpp"
#include "util/Math.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace artouste::render {

class SkinnedModel {
public:
    /* Nombre maximal d'os par variante, borne de l'uniforme u_bones du shader.
       Les squelettes du pack en comptent ~40, cette marge est confortable. */
    static constexpr int MAX_BONES = 64;

    /* Un sommet skinné : jusqu'à quatre os influents (indices dans la liste
       d'os de SA variante) et leurs poids (normalisés, somme ~1). */
    struct SkinnedVertex {
        vec3  position{0.0f};
        vec3  normal{0.0f, 1.0f, 0.0f};
        vec2  uv{0.0f, 0.0f};
        int   joints[4]  = {0, 0, 0, 0};
        float weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    /* Une variante de personnage (femme A, homme C...) : sa géométrie et son
       squelette propres. */
    struct MeshData {
        std::vector<SkinnedVertex> vertices;
        std::vector<unsigned int>  indices;
        std::vector<int>           boneNode;    /* index du noeud dans m_nodes, par os */
        std::vector<mat4>          boneOffset;  /* matrice de bind inverse, par os */
        mat4                       localFix{1.0f};  /* recentrage + cm->m propre à la variante */
    };

    explicit SkinnedModel(const std::filesystem::path& path);

    [[nodiscard]] bool            built() const noexcept { return m_built; }
    [[nodiscard]] std::size_t     meshCount() const noexcept { return m_meshes.size(); }
    [[nodiscard]] const MeshData& mesh(std::size_t i) const { return m_meshes[i]; }
    [[nodiscard]] const Texture*  texture() const noexcept { return m_texture.get(); }
    [[nodiscard]] float           durationS() const noexcept { return m_durationS; }

    /* Pose tout le squelette à l'instant t (secondes, bouclé sur durationS) :
       renvoie la matrice globale (repère du modèle) de chaque noeud, dans
       l'ordre de m_nodes (un parent précède toujours ses enfants). */
    [[nodiscard]] std::vector<mat4> poseAtTime(float t) const;

    /* Matrices d'os finales d'une variante à partir des poses de poseAtTime :
       out[b] = localFix * globalInverse * globalNoeud(os_b) * offset_b. Prêtes
       telles quelles pour l'uniforme u_bones (la correction localFix est déjà
       intégrée, le shader n'a donc qu'à appliquer le skinning puis la matrice
       d'instance). Taille = nombre d'os de la variante (<= MAX_BONES). */
    [[nodiscard]] std::vector<mat4> boneMatrices(std::size_t meshIndex,
                                                 const std::vector<mat4>& globals) const;

    /* Décalage horizontal (X,Z en repère final) du centre de la variante a
       l'instant t, dû au "root motion" de l'animation (le personnage se déplace
       dans le cycle). Nul a t=0 (localFix y recentre) ; ailleurs, le rendu le
       retranche pour épingler le zombie sur sa position logique et supprimer le
       glissement. Interpolé depuis une petite table précalculée ; t est bouclé
       comme dans poseAtTime. */
    [[nodiscard]] vec2 rootDriftXZ(std::size_t meshIndex, float t) const;

private:
    /* Un noeud du squelette : sa transformation locale par défaut, son parent
       (index dans m_nodes, -1 pour la racine) et son canal d'animation
       éventuel (-1 si le noeud n'est pas animé). */
    struct Node {
        mat4 localDefault{1.0f};
        int  parent  = -1;
        int  channel = -1;
    };

    /* Canal d'animation d'un noeud : trois pistes de clés (translation,
       rotation, échelle), horodatées en secondes. */
    struct Channel {
        std::vector<std::pair<float, vec3>> posKeys;
        std::vector<std::pair<float, quat>> rotKeys;
        std::vector<std::pair<float, vec3>> scaleKeys;
    };

    /* Nombre d'échantillons de la table de dérive (root motion) par variante. */
    static constexpr int CENTER_SAMPLES = 32;

    std::vector<Node>        m_nodes;
    std::vector<Channel>     m_channels;
    std::vector<MeshData>    m_meshes;
    /* Décalage (X,Z final) du centre par variante, échantillonné sur l'animation
       (CENTER_SAMPLES points, dernier = premier pour boucler proprement). */
    std::vector<std::vector<vec2>> m_centerTable;
    std::unique_ptr<Texture> m_texture;
    mat4                     m_globalInverse{1.0f};
    float                    m_durationS = 0.0f;
    bool                     m_built     = false;
};

}  /* namespace artouste::render */
