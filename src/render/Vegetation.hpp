/*
 * Vegetation.hpp
 * Végétation en billboards instanciés (prototype). À partir de l'orthophoto du
 * terrain, on sème des arbres là où le sol est vert et sous la limite
 * forestière, chacun posé sur le relief. Un seul quad de base est dessiné des
 * milliers de fois par instanciation GPU (une position + une échelle par arbre).
 *
 * Approche volontairement simple, destinée à juger le rendu et les performances
 * avant d'investir dans un pipeline hors-ligne (positions précalculées) et des
 * niveaux de détail. Voir ROADMAP.md, section Végétation.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Texture.hpp"

#include <cstddef>
#include <filesystem>

namespace artouste::render {

class Terrain;

class Vegetation {
public:
    /* Sème la végétation sur le terrain à partir de son orthophoto (dossier du
       terrain) et du sprite d'arbre donné. Sans orthophoto ou sans sprite, l'objet
       reste vide (built() faux) et draw() ne fait rien. */
    Vegetation(const std::filesystem::path& terrainDir, const Terrain& terrain,
               const std::filesystem::path& spritePath);
    ~Vegetation();

    Vegetation(const Vegetation&)            = delete;
    Vegetation& operator=(const Vegetation&) = delete;

    /* Dessine tous les arbres (instancié). La texture est liée sur l'unité 0 ;
       le shader de végétation doit être actif et ses uniformes déjà renseignés. */
    void draw() const;

    [[nodiscard]] bool        built() const noexcept { return m_count > 0; }
    [[nodiscard]] std::size_t count() const noexcept { return m_count; }

private:
    void release() noexcept;

    Texture       m_sprite;
    unsigned int  m_vao          = 0;
    unsigned int  m_quadVbo      = 0;
    unsigned int  m_instanceVbo  = 0;
    unsigned int  m_ebo          = 0;
    std::size_t   m_count        = 0;  /* nombre d'arbres semés */
};

}  /* namespace artouste::render */
