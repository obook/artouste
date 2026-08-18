/*
 * FenetreReliefInterne.hpp
 * Constantes du format de tuile et petits lecteurs, partagés par les fichiers
 * de la fenêtre de relief.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace artouste::render::relief {

/* En-tête d'une tuile, tel que tools/terrain/fetch_relief.py l'écrit : 20 octets
   petit-boutiens, puis côté x côté entiers 16 bits. */
constexpr char        MAGIQUE[4]        = {'A', 'R', 'T', 'R'};
constexpr std::size_t EN_TETE_OCTETS    = 20; /* v1 : un seul pas */
constexpr std::size_t EN_TETE_V2_OCTETS = 24; /* v2 : un pas par axe */

/* Reprise du ruban de triangles. Un ruban par rangée : 8 Mo d'indices au lieu de
   25 pour une grille de 1024. */
constexpr unsigned int REPRISE = 0xFFFFFFFFu;

/* Reste toujours positif : un point à l'ouest de l'ancre a un indice négatif. */
[[nodiscard]] inline int modulo(int a, int b) noexcept {
    const int r = a % b;
    return (r < 0) ? r + b : r;
}

/* Lit un entier 16 bits petit-boutien. */
[[nodiscard]] inline std::uint16_t lire16(const unsigned char* p) noexcept {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8);
}

/* Lit un flottant 32 bits petit-boutien. */
[[nodiscard]] inline float lireFlottant(const unsigned char* p) noexcept {
    float valeur = 0.0f;
    std::memcpy(&valeur, p, sizeof(valeur));
    return valeur;
}

} /* namespace artouste::render::relief */
