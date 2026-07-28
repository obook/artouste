/*
 * ZombieEyes.hpp
 * Lueur des yeux des zombies : billboards face caméra instanciés, deux par
 * zombie, dessinés en mélange additif après les personnages. Même construction
 * que render::combat::Projectiles (un quad, un tampon d'instances réécrit
 * chaque image), avec une couleur par instance en plus : c'est elle qui
 * distingue un marcheur (vert) d'une pondeuse (rouge).
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

class ZombieEyes {
public:
    /* Une lueur : centre MONDE (avant recalage repère-caméra, voir u_model du
       shader), rayon de près en mètres, couleur et intensité. */
    struct Instance {
        vec4 posRadius{0.0f};
        vec4 color{0.0f};  /* w inutilisé, présent pour aligner l'attribut sur un vec4 */
    };

    /* Prépare un tampon d'instances dynamique pour au plus 'capacity' lueurs
       simultanées (soit deux fois le nombre de zombies affichables). */
    explicit ZombieEyes(std::size_t capacity);
    ~ZombieEyes();

    ZombieEyes(const ZombieEyes&)            = delete;
    ZombieEyes& operator=(const ZombieEyes&) = delete;

    /* Réécrit le tampon d'instances. Ignore silencieusement au-delà de
       'capacity'. */
    void updateInstances(const std::vector<Instance>& instances);

    /* Dessine toutes les instances courantes (shader déjà actif, uniformes
       u_model/u_view/u_proj/u_camPos déjà renseignées). Pose et restaure
       lui-même le mélange additif et le masque de profondeur, comme
       render::ExplosionFx. */
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
