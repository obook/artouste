/*
 * Projectiles.hpp
 * Boulettes toxiques en billboard face caméra, instanciées, sans texture (la
 * forme est procédurale, voir projectile.frag) : plus simple que
 * render::combat::SkinnedZombies (pas de modèle importé ni de squelette, juste
 * une position et une échelle par instance), tampon d'instances mis à jour
 * chaque image (GL_DYNAMIC_DRAW).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <cstddef>
#include <vector>

namespace artouste::render {

class Projectiles {
public:
    /* Prépare un tampon d'instances dynamique pour au plus 'capacity'
       boulettes simultanées. */
    explicit Projectiles(std::size_t capacity);
    ~Projectiles();

    Projectiles(const Projectiles&)            = delete;
    Projectiles& operator=(const Projectiles&) = delete;

    /* Réécrit le tampon d'instances : xyz = centre MONDE (avant recalage
       repère-caméra, voir u_model du shader), w = échelle (diamètre, m).
       Ignore silencieusement au-delà de 'capacity'. */
    void updateInstances(const std::vector<vec4>& instances);

    /* Dessine toutes les instances courantes (shader déjà actif, uniformes
       u_model/u_view/u_proj déjà renseignées). */
    void draw() const;

    [[nodiscard]] bool built() const noexcept { return m_built; }

private:
    void release() noexcept;

    unsigned int m_vao         = 0;
    unsigned int m_quadVbo     = 0;
    unsigned int m_ebo         = 0;
    unsigned int m_instanceVbo = 0;
    std::size_t  m_capacity    = 0;
    std::size_t  m_count       = 0;
    bool         m_built       = false;
};

}  /* namespace artouste::render */
