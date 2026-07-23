/*
 * ExplosionModel.hpp
 * Explosion 3D animée par NŒUDS (pas de squelette), chargée depuis un glTF/.glb
 * via Assimp. Le pack utilisé est une séquence façon "flipbook 3D" : une
 * douzaine de maillages (les images successives de la boule de feu), chacun
 * porté par un nœud dont l'ÉCHELLE est animée (il grossit puis s'efface à tour
 * de rôle). On réutilise la même mécanique d'animation de nœuds que
 * render::SkinnedModel (poseAtTime), mais sans os ni skinning : chaque maillage
 * est simplement dessiné avec la transformation globale animée de son nœud.
 *
 * Le modèle expose :
 *   - la liste des maillages (géométrie statique + index du nœud porteur) ;
 *   - poseAtTime(t) : matrice globale de chaque nœud à l'instant t ;
 *   - une correction localFix normalisant l'explosion à un rayon unité (le
 *     renderer la met ensuite à l'échelle voulue et la place au point d'impact).
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
    /* Un maillage (une image de l'explosion) et le nœud qui l'anime. */
    struct MeshPart {
        Mesh mesh;
        int  node = 0;  /* index dans m_nodes du nœud porteur */
    };

    explicit ExplosionModel(const std::filesystem::path& path);

    [[nodiscard]] bool           built() const noexcept { return m_built; }
    [[nodiscard]] std::size_t    partCount() const noexcept { return m_parts.size(); }
    [[nodiscard]] const MeshPart& part(std::size_t i) const { return m_parts[i]; }
    [[nodiscard]] const Texture*  texture() const noexcept { return m_texture.get(); }
    [[nodiscard]] const mat4&     localFix() const noexcept { return m_localFix; }
    [[nodiscard]] float           durationS() const noexcept { return m_durationS; }

    /* Matrice globale (repère du modèle) de chaque nœud à l'instant t
       (secondes, bornée à [0, durationS]). Ordre de m_nodes : un parent précède
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
