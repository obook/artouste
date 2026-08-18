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
#include <utility>
#include <vector>

namespace artouste::render {

class Terrain;

class Vegetation {
public:
    /* Sème la végétation sur le terrain à partir de son orthophoto (dossier du
       terrain) et du sprite d'arbre donné. treeBudget borne le nombre d'arbres
       soumis au GPU (au-delà, le semis est éclairci uniformément) : c'est le levier
       de performance principal. Sans orthophoto ou sans sprite, l'objet reste vide
       (built() faux) et draw() ne fait rien. */
    Vegetation(const std::filesystem::path& terrainDir,
               const Terrain& terrain,
               const std::filesystem::path& spritePath,
               std::size_t treeBudget);
    ~Vegetation();

    Vegetation(const Vegetation&) = delete;
    Vegetation& operator=(const Vegetation&) = delete;

    /* Dessine tous les arbres (instancié). La texture est liée sur l'unité 0 ;
       le shader de végétation doit être actif et ses uniformes déjà renseignés. */
    void draw() const;

    [[nodiscard]] bool built() const noexcept { return m_count > 0; }
    [[nodiscard]] std::size_t count() const noexcept { return m_count; }

private:
    void release() noexcept;

    /* Zone circulaire (aérodromes, etc.) où l'on ne plante aucun arbre. */
    struct Exclusion {
        float x, z, r2;
    };

    /* Codes du masque BD Forêt (voir VegetationForestMask.cpp). "Hors France"
       n'est pas "pas de forêt" : la BD Forêt s'arrête à la frontière, le semis y
       retombe donc sur la couleur de l'ortho. En France, au contraire, elle fait
       autorité : ce qu'elle ne dessine pas est ForestNonBoise. */
    enum ForestClass : unsigned char {
        ForestHorsFrance = 0,
        ForestNonBoise = 1,
        ForestFeuillu = 2,
        ForestPin = 3,
        ForestConifere = 4,
        ForestMixte = 5,
    };

    /* Conversion d'une position monde (x est, z sud) en pixel de l'orthophoto
       (colonne 0 = ouest, rangée 0 = nord). Partagée par le masquage et le semis. */
    static void toPixel(float x,
                        float z,
                        float halfW,
                        float halfH,
                        int orthoW,
                        int orthoH,
                        int& ox,
                        int& oy) noexcept;
    /* Couleur normalisée (0..1) du pixel (ox, oy) de l'orthophoto RGB. */
    static void orthoRGB(const unsigned char* ortho,
                         int orthoW,
                         int ox,
                         int oy,
                         float& r,
                         float& g,
                         float& b) noexcept;

    /* Masque d'eau (flood fill depuis les repères "Lac" de landmarks.txt, puis
       dilaté pour dégager la rive). Les lacs sans graine d'eau trouvée sous leur
       repère sont rangés dans fallbackLakes (dégagés par un disque de secours au
       semis). Voir VegetationWaterMask.cpp. */
    std::vector<unsigned char>
    buildWaterMask(const std::filesystem::path& terrainDir,
                   const Terrain& terrain,
                   const unsigned char* ortho,
                   int orthoW,
                   int orthoH,
                   float halfW,
                   float halfH,
                   std::vector<std::pair<float, float>>& fallbackLakes) const;
    /* Masque d'emprise des bâtiments (buildings.bin, rastérisé à la résolution de
       l'orthophoto) : aucun arbre n'y est planté. Voir VegetationBuildingMask.cpp. */
    std::vector<unsigned char> buildBuildingMask(const std::filesystem::path& terrainDir,
                                                 const Terrain& terrain,
                                                 int orthoW,
                                                 int orthoH,
                                                 float halfW,
                                                 float halfH) const;
    /* Masque de forêt de la BD Forêt IGN (forest.png, facultatif), rendu à sa
       propre résolution : forestW/forestH la renseignent. Vide si le fichier est
       absent (le semis retombe alors sur la couleur de l'ortho, comme avant).
       Voir VegetationForestMask.cpp. */
    std::vector<unsigned char> buildForestMask(const std::filesystem::path& terrainDir,
                                               int& forestW,
                                               int& forestH) const;
    /* Zones d'exclusion (exclusions.txt, facultatif). Voir VegetationMasks.cpp. */
    std::vector<Exclusion> loadExclusions(const std::filesystem::path& terrainDir,
                                          const Terrain& terrain) const;

    /* Boucle de placement sur la grille de semis, puis éclaircissement au budget si
       le nombre d'arbres dépasse TARGET_TREES. Renvoie le tampon d'instances (six
       flottants par arbre : centre, largeur, espèce, azimut). Voir
       VegetationScatter.cpp. */
    std::vector<float>
    scatterTrees(const Terrain& terrain,
                 const unsigned char* ortho,
                 int orthoW,
                 int orthoH,
                 float halfW,
                 float halfH,
                 float spacing,
                 bool clear,
                 float sx,
                 float sz,
                 const std::vector<unsigned char>& water,
                 const std::vector<unsigned char>& building,
                 const std::vector<unsigned char>& forest,
                 int forestW,
                 int forestH,
                 const std::vector<Exclusion>& exclusions,
                 const std::vector<std::pair<float, float>>& fallbackLakes) const;

    /* Téléverse la géométrie de base (billboard en croix) et le tampon d'instances
       dans un VAO/VBO/EBO. */
    void uploadGpuBuffers(const std::vector<float>& instances);

    Texture m_sprite;
    unsigned int m_vao = 0;
    unsigned int m_quadVbo = 0;
    unsigned int m_instanceVbo = 0;
    unsigned int m_ebo = 0;
    std::size_t m_count = 0;  /* nombre d'arbres semés */
    /* Ramène le semis au budget d'arbres, si celui-ci est dépassé. Défini dans
       vegetation/VegetationBudget.cpp. */
    void eclaircirAuBudget(std::vector<float>& instances, std::size_t count) const;

    std::size_t m_budget = 0; /* plafond d'arbres (voir scatterTrees) */
};

} /* namespace artouste::render */
