/*
 * Lampadaires.hpp
 * Éclairage public d'une carte : un point lumineux par lampadaire
 * (lampadaires.bin, voir tools/fetch_lampadaires.py), dessiné la nuit en
 * GL_POINTS additifs. Vu d'hélicoptère, c'est ce chapelet de points orangés
 * qui dessine les rues, pas la lumière au sol.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace artouste::render {

class Terrain;

class Lampadaires {
public:
    /* Charge lampadaires.bin depuis dir et pose chaque point sur le relief, à
       HAUTEUR_M au-dessus du sol, puis ajoute les feux donnés tels quels
       (x, y, z monde et type, voir lampadaires.vert). Rien du tout : built()
       == false. */
    Lampadaires(const std::filesystem::path& dir, const Terrain& terrain,
                const std::vector<vec4>& feux = {});
    ~Lampadaires();
    Lampadaires(const Lampadaires&)            = delete;
    Lampadaires& operator=(const Lampadaires&) = delete;

    /* Dessine tous les points (shader lampadaires actif, uniformes posées).
       Pose et restaure lui-même le mélange additif et le masque de
       profondeur. */
    void draw() const;

    [[nodiscard]] bool        built() const noexcept { return m_count > 0; }
    [[nodiscard]] std::size_t count() const noexcept { return m_count; }

private:
    unsigned int m_vao   = 0;
    unsigned int m_vbo   = 0;
    std::size_t  m_count = 0;
};

} /* namespace artouste::render */
