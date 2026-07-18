/*
 * ExplosionModel.hpp
 * Explosion 3D animée par NOEUDS (pas de squelette), chargée depuis un glTF/.glb
 * via Assimp. Le pack utilisé est une séquence facon "flipbook 3D" : une
 * douzaine de maillages (les images successives de la boule de feu), chacun
 * porté par un noeud dont l'ECHELLE est animée (il grossit puis s'efface a tour
 * de role). On réutilise la même mécanique d'animation de noeuds que
 * render::SkinnedModel (poseAtTime), mais sans os ni skinning : chaque maillage
 * est simplement dessiné avec la transformation globale animée de son noeud.
 *
 * Le modele expose :
 *   - la liste des maillages (géométrie statique + index du noeud porteur) ;
 *   - poseAtTime(t) : matrice globale de chaque noeud a l'instant t ;
 *   - une correction localFix normalisant l'explosion a un rayon unité (le
 *     renderer la met ensuite a l'échelle voulue et la place au point d'impact).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Mesh.hpp"
#include "render/Texture.hpp"
#include "util/Math.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace artouste::render {

class ExplosionModel {
public:
    /* Un maillage (une image de l'explosion) et le noeud qui l'anime. */
    struct MeshPart {
        Mesh mesh;
        int  node = 0;  /* index dans m_nodes du noeud porteur */
    };

    explicit ExplosionModel(const std::filesystem::path& path);

    [[nodiscard]] bool           built() const noexcept { return m_built; }
    [[nodiscard]] std::size_t    partCount() const noexcept { return m_parts.size(); }
    [[nodiscard]] const MeshPart& part(std::size_t i) const { return m_parts[i]; }
    [[nodiscard]] const Texture*  texture() const noexcept { return m_texture.get(); }
    [[nodiscard]] const mat4&     localFix() const noexcept { return m_localFix; }
    [[nodiscard]] float           durationS() const noexcept { return m_durationS; }

    /* Matrice globale (repere du modele) de chaque noeud a l'instant t
       (secondes, borne a [0, durationS]). Ordre de m_nodes : un parent precede
       toujours ses enfants. */
    [[nodiscard]] std::vector<mat4> poseAtTime(float t) const;

private:
    struct Node {
        mat4 localDefault{1.0f};
        int  parent  = -1;
        int  channel = -1;
    };
    struct Channel {
        std::vector<std::pair<float, vec3>> posKeys;
        std::vector<std::pair<float, quat>> rotKeys;
        std::vector<std::pair<float, vec3>> scaleKeys;
    };

    std::vector<Node>        m_nodes;
    std::vector<Channel>     m_channels;
    std::vector<MeshPart>    m_parts;
    std::unique_ptr<Texture> m_texture;
    mat4                     m_localFix{1.0f};
    float                    m_durationS = 0.0f;
    bool                     m_built     = false;
};

}  /* namespace artouste::render */
