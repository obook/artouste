/*
 * Vegetation.cpp
 * Sème des arbres en billboards sur le terrain à partir de son orthophoto, puis
 * les dessine par instanciation GPU. Un arbre est retenu là où le sol est vert
 * (signature de couleur de la forêt dans l'ortho) et sous la limite forestière ;
 * il est posé sur le relief (heightAt). Toutes les positions sont téléversées une
 * fois dans un tampon d'instances ; le rendu dessine un unique quad autant de
 * fois qu'il y a d'arbres.
 *
 * Orchestration du semis : le masquage (eau, bâtiments, exclusions) vit dans
 * VegetationMasks.cpp, la boucle de placement et l'éclaircissement au budget
 * dans VegetationScatter.cpp. Ce fichier charge l'orthophoto, enchaîne ces
 * étapes et téléverse le résultat sur GPU.
 *
 * Prototype : semis simple et brume identique aux bâtiments, pour juger le rendu
 * et les performances avant un pipeline hors-ligne et des niveaux de détail.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/Vegetation.hpp"

#include "render/Terrain.hpp"

#include <glad/glad.h>

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace artouste::render {

namespace {

/* Espacement de la grille de semis (m) : un arbre candidat tous les SPACING_M,
   avec une position perturbée dans la maille. Comme chaque maille ne donne au plus
   qu'un arbre, l'espacement fixe la densité maximale (1 arbre par SPACING_M^2) :
   à 8 m -> ~150 arbres/ha, un couvert crédible sans exploser le nombre. Réglable
   via ARTOUSTE_TREE_SPACING pour tâtonner sans recompiler (plus petit = plus dense
   = plus lourd ; une vraie forêt tourne autour de 4-5 m). */
constexpr float SPACING_M_DEFAULT = 8.0f;

}  /* namespace */

void Vegetation::toPixel(float x, float z, float halfW, float halfH, int orthoW, int orthoH,
                         int& ox, int& oy) noexcept {
    const float u = (x + halfW) / (2.0f * halfW);
    const float v = (z + halfH) / (2.0f * halfH);
    ox = std::min(std::max(static_cast<int>(u * static_cast<float>(orthoW - 1)), 0), orthoW - 1);
    oy = std::min(std::max(static_cast<int>(v * static_cast<float>(orthoH - 1)), 0), orthoH - 1);
}

void Vegetation::orthoRGB(const unsigned char* ortho, int orthoW, int ox, int oy,
                          float& r, float& g, float& b) noexcept {
    const unsigned char* p = ortho
                           + (static_cast<std::size_t>(oy) * static_cast<std::size_t>(orthoW)
                              + static_cast<std::size_t>(ox)) * 3;
    r = p[0] / 255.0f;
    g = p[1] / 255.0f;
    b = p[2] / 255.0f;
}

Vegetation::Vegetation(const std::filesystem::path& terrainDir, const Terrain& terrain,
                       const std::filesystem::path& spritePath, std::size_t treeBudget)
    : m_budget(treeBudget) {
    /* Sprite d'arbre : sans lui, rien à dessiner. */
    m_sprite = Texture(spritePath);
    if (!m_sprite.valid()) {
        std::fprintf(stderr, "[Vegetation] sprite absent (%s) : végétation ignorée.\n",
                     spritePath.string().c_str());
        return;
    }

    /* Orthophoto côté CPU (mêmes conventions que Buildings : rangée 0 = nord). */
    const std::filesystem::path orthoPath = terrainDir / "ortho.jpg";
    int            orthoW = 0, orthoH = 0, orthoCh = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* ortho = stbi_load(orthoPath.string().c_str(), &orthoW, &orthoH, &orthoCh, 3);
    if (ortho == nullptr) {
        std::fprintf(stderr, "[Vegetation] orthophoto absente (%s) : végétation ignorée.\n",
                     orthoPath.string().c_str());
        return;
    }

    /* Espacement de la grille, éventuellement forcé par l'environnement. */
    float spacing = SPACING_M_DEFAULT;
    if (const char* env = std::getenv("ARTOUSTE_TREE_SPACING"); env != nullptr && env[0] != '\0') {
        const float v = std::strtof(env, nullptr);
        if (v >= 4.0f) {
            spacing = v;
        }
    }

    const float halfW = terrain.halfWidth();
    const float halfH = terrain.halfHeight();
    const bool  clear = terrain.hasStart();
    const float sx    = clear ? terrain.startX() : 0.0f;
    const float sz    = clear ? terrain.startZ() : 0.0f;

    /* Masquage : eau (avec repli sur les lacs sans graine trouvée), bâtiments,
       zones d'exclusion. Voir VegetationMasks.cpp. */
    std::vector<std::pair<float, float>> fallbackLakes;
    const std::vector<unsigned char> water =
        buildWaterMask(terrainDir, terrain, ortho, orthoW, orthoH, halfW, halfH, fallbackLakes);
    const std::vector<unsigned char> building =
        buildBuildingMask(terrainDir, terrain, orthoW, orthoH, halfW, halfH);
    const std::vector<Exclusion> exclusions = loadExclusions(terrainDir, terrain);

    /* Placement sur la grille de semis et éclaircissement au budget. Voir
       VegetationScatter.cpp. */
    const std::vector<float> instances =
        scatterTrees(terrain, ortho, orthoW, orthoH, halfW, halfH, spacing, clear, sx, sz,
                     water, building, exclusions, fallbackLakes);

    stbi_image_free(ortho);

    m_count = instances.size() / 6;
    if (m_count == 0) {
        return;
    }

    uploadGpuBuffers(instances);

    std::printf("[Vegetation] %zu arbres semés (espacement %.0f m).\n", m_count,
                static_cast<double>(spacing));
}

void Vegetation::uploadGpuBuffers(const std::vector<float>& instances) {
    /* Géométrie de base : deux quads verticaux (billboard en croix). Par sommet :
       coin (x dans [-0.5,0.5], y dans [0,1]), UV, et plan (0 ou 1). Le shader
       oriente chaque plan dans le monde selon l'azimut de l'instance. */
    const float quads[] = {
        /* corner.x corner.y   u    v   plane */
        /* plan 0 */
        -0.5f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.5f, 1.0f, 1.0f, 1.0f, 0.0f,
        -0.5f, 1.0f, 0.0f, 1.0f, 0.0f,
        /* plan 1 (perpendiculaire, posé par le shader) */
        -0.5f, 0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, 1.0f, 1.0f, 1.0f, 1.0f,
        -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,
    };
    const unsigned int indices[] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    /* Sommets : attributs 0 (coin), 1 (UV), 2 (plan). Cinq flottants par sommet. */
    constexpr GLsizei vstride = 5 * sizeof(float);
    glGenBuffers(1, &m_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quads), quads, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vstride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vstride,
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, vstride,
                          reinterpret_cast<void*>(4 * sizeof(float)));

    /* Indices (stockés dans le VAO). */
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    /* Tampon d'instances : attributs 3 (centre), 4 (largeur), 5 (espèce), 6 (azimut),
       un par arbre. Six flottants par instance (stride 24 octets). */
    constexpr GLsizei istride = 6 * sizeof(float);
    glGenBuffers(1, &m_instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(instances.size() * sizeof(float)), instances.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, istride, reinterpret_cast<void*>(0));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, istride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, istride,
                          reinterpret_cast<void*>(4 * sizeof(float)));
    glVertexAttribDivisor(5, 1);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, istride,
                          reinterpret_cast<void*>(5 * sizeof(float)));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Vegetation::~Vegetation() {
    release();
}

void Vegetation::release() noexcept {
    if (m_instanceVbo != 0) {
        glDeleteBuffers(1, &m_instanceVbo);
    }
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
    }
    if (m_quadVbo != 0) {
        glDeleteBuffers(1, &m_quadVbo);
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    m_vao = m_quadVbo = m_instanceVbo = m_ebo = 0;
    m_count = 0;
}

void Vegetation::draw() const {
    if (m_count == 0) {
        return;
    }
    m_sprite.bind(0);
    glBindVertexArray(m_vao);
    /* 12 indices = les deux quads de la croix ; une instance par arbre. */
    glDrawElementsInstanced(GL_TRIANGLES, 12, GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(m_count));
    glBindVertexArray(0);
}

}  /* namespace artouste::render */
