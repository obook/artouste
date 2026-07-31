/*
 * SouffleFx.hpp
 * Rendu du souffle rotor : les bouffées de poussière de app::SouffleRotor,
 * dessinées en billboards face caméra, instanciés, sans texture (le grain est
 * procédural, voir souffle.frag). Tampon d'instances réécrit à chaque image,
 * comme render::Projectiles, puisque le nuage change entièrement d'une image à
 * l'autre.
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

class SouffleFx {
public:
    /* Une bouffée à dessiner. Deux vecteurs de quatre flottants, qui sont aussi
       les deux attributs d'instance du shader. */
    struct Instance {
        vec4 centreDiametre{0.0f}; /* xyz = centre MONDE, w = diamètre (m) */
        vec4 grain{0.0f}; /* x = opacité, y = graine, z = rotation (rad), w = hauteur sol (m) */
    };

    /* Prépare un tampon d'instances dynamique pour au plus 'capacite' bouffées
       simultanées. */
    explicit SouffleFx(std::size_t capacite);
    ~SouffleFx();

    SouffleFx(const SouffleFx&) = delete;
    SouffleFx& operator=(const SouffleFx&) = delete;

    /* Réécrit le tampon d'instances. Ignore silencieusement au-delà de
       'capacite'. */
    void updateInstances(const std::vector<Instance>& instances);

    /* Dessine toutes les instances courantes. Le shader doit être actif et ses
       uniformes déjà renseignés ; l'appelant gère l'état de mélange. */
    void draw() const;

    [[nodiscard]] bool built() const noexcept { return m_built; }
    [[nodiscard]] std::size_t capacite() const noexcept { return m_capacite; }

private:
    void release() noexcept;

    unsigned int m_vao = 0;
    unsigned int m_quadVbo = 0;
    unsigned int m_ebo = 0;
    unsigned int m_instanceVbo = 0;
    std::size_t m_capacite = 0;
    std::size_t m_count = 0;
    bool m_built = false;
};

} /* namespace artouste::render */
