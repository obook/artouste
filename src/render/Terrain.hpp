/*
 * Terrain.hpp
 * Terrain réel de la côte basque (Hendaye -> Biarritz). Le relief vient d'une
 * carte d'altitude (heightmap) issue du MNT de l'IGN, et l'orthophoto réelle
 * est drapée dessus comme une texture. Si les données sont absentes, on retombe
 * sur un simple damier plat afin que le simulateur reste utilisable.
 *
 * Repère monde du projet : X vers l'est, Z vers le sud, Y vers le haut. Le
 * terrain est centré sur l'origine.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include "render/Mesh.hpp"
#include "render/Texture.hpp"

#include <filesystem>
#include <vector>

namespace artouste::render {

class Terrain {
public:
    /* Charge le terrain depuis un dossier contenant terrain.txt, heightmap.png
       et ortho.jpg. En cas d'absence, construit un damier plat de repli. */
    explicit Terrain(const std::filesystem::path& dir);

    void draw() const { m_mesh.draw(); }

    /* Vrai si l'orthophoto est chargée : le rendu doit alors utiliser le shader
       de terrain texturé ; sinon, le shader à couleurs de sommets suffit. */
    [[nodiscard]] bool textured() const noexcept { return m_textured; }

    /* Active l'orthophoto sur l'unité de texture donnée. */
    void bindTexture(unsigned int unit = 0) const { m_ortho.bind(unit); }

    /* Altitude du sol (m) sous le point (x, z) du repère monde, par
       interpolation bilinéaire. Renvoie 0 (niveau de la mer) hors de l'emprise. */
    [[nodiscard]] float heightAt(float x, float z) const noexcept;

    [[nodiscard]] float halfWidth() const noexcept { return 0.5f * m_widthM; }
    [[nodiscard]] float halfHeight() const noexcept { return 0.5f * m_heightM; }
    [[nodiscard]] float maxElevation() const noexcept { return m_elevMax; }

private:
    void buildFlatFallback();

    Mesh    m_mesh;
    Texture m_ortho;
    bool    m_textured = false;

    /* Carte d'altitude conservée côté CPU pour interroger la hauteur du sol.
       Rangée 0 = nord (latitude max), colonne 0 = ouest (longitude min). */
    std::vector<float> m_heights;
    int                m_cols    = 0;
    int                m_rows    = 0;
    float              m_widthM  = 0.0f;  /* dimension est-ouest au sol (m) */
    float              m_heightM = 0.0f;  /* dimension nord-sud au sol (m) */
    float              m_elevMin = 0.0f;
    float              m_elevMax = 0.0f;
};

}  /* namespace artouste::render */
