/*
 * BuildingsCouleurs.hpp
 * Couleur des murs et des toits, lue dans l'orthophoto de la carte.
 *
 * Une prise de vue au nadir montre le toit lui-même au centre de l'emprise :
 * chaque bâtiment reçoit sa vraie teinte, le zinc parisien comme la tuile
 * landaise, sans rien déclarer par carte. La même image sert à repérer l'eau
 * sous les cabanes des ports.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <cstdint>
#include <filesystem>

namespace artouste::render {

class Terrain;

/* Enduit clair, façon côte basque et Landes. */
extern const vec3 WALL_COLOR;

/* Facteur dans [1-amp, 1+amp] tiré d'un entier : nuance une couleur sans état
   global et sans scintiller d'une image à l'autre. */
[[nodiscard]] float jitter(std::uint32_t seed, float amp);

/* Orthophoto de la carte, chargée côté CPU le temps de la construction. */
class Ortho {
public:
    Ortho(const std::filesystem::path& image, const Terrain& terrain);
    ~Ortho();

    Ortho(const Ortho&)            = delete;
    Ortho& operator=(const Ortho&) = delete;

    [[nodiscard]] bool presente() const noexcept { return m_pixels != nullptr; }

    /* Teinte du toit d'un bâtiment. L'emprise doit couvrir assez de pixels pour
       que ce soit bien le toit qu'on y lise : les orthophotos vont de 0,85 m/px
       à 9,8 m/px, et au pied de cette échelle un pixel couvre la maison, son
       jardin et ses pins. Trop petite, ou pas d'ortho : palette de repli. */
    [[nodiscard]] vec3 couleurToit(float cx,
                                   float cz,
                                   float largeurM,
                                   float profondeurM,
                                   std::uint32_t seed) const;

    /* Cabane bâtie sur l'eau (port ostréicole) : sol bas ET couleur d'eau.
       L'eau a un rouge faible et un bleu nettement supérieur, ce qui la
       distingue des ombres et des toits sombres comme des terres habitées. La
       seule altitude ne suffirait pas, la carte de relief rate les chenaux
       étroits des ports. */
    [[nodiscard]] bool surLeau(float x, float z, float altitudeSol) const;

private:
    [[nodiscard]] const unsigned char* pixel(float x, float z) const;

    unsigned char* m_pixels = nullptr;
    int            m_w      = 0;
    int            m_h      = 0;

    /* Emprise du terrain, pour convertir une position monde en pixel. */
    float m_halfW = 0.0f;
    float m_halfH = 0.0f;
    float m_origX = 0.0f;
    float m_origZ = 0.0f;

    /* Taille au sol d'un pixel, en mètres. */
    float m_pixelX = 0.0f;
    float m_pixelZ = 0.0f;

    /* Mordant de CETTE image : une ortho grise est étalée plus fort qu'une
       image franche, sinon ses toits forment une nappe uniforme. */
    float m_luminosite = 0.45f;
    float m_contraste  = 1.0f;
};

} /* namespace artouste::render */
