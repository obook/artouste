/*
 * VegetationCouleurs.hpp
 * Hachage déterministe du semis, et lecture de la couleur du sol pour décider
 * où planter.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include <cstdint>

namespace artouste::render {

/* Limite forestière progressive (m). Une forêt de montagne ne s'arrête pas net :
   elle se raréfie (pins isolés) avant de céder la place à la pelouse d'altitude. On
   garde donc le couvert plein sous TREELINE_FULL, on raréfie linéairement jusqu'à
   TREELINE_MAX, et plus rien au-dessus. Évite la ligne de coupure brutale que
   donnait une limite unique, et pose des arbres épars sur les hautes pentes vertes.
   Pyrénées : couvert dense jusque ~1900 m, derniers pins à crochets vers ~2200 m. */
constexpr float TREELINE_FULL = 1900.0f;
constexpr float TREELINE_MAX  = 2200.0f;

/* Taille des arbres : largeur du billboard (m) ; la hauteur en découle dans le
   shader (facteur ASPECT). Panachée par arbre pour éviter l'uniformité. */
constexpr float TREE_WIDTH_MIN = 4.0f;
constexpr float TREE_WIDTH_MAX = 7.0f;

/* Dégagement de secours autour du repère de départ de terrain.txt (rayon au
   carré, m^2), pour une carte sans helipads.txt : le départ s'y fait sur le
   repère brut. Les cartes qui ont des hélipads sont dégagées par les zones
   d'exclusion, pad par pad (voir VegetationMasks.cpp). */
constexpr float CLEAR_R2 = 50.0f * 50.0f;

/* Petit dégagement de secours autour du repère d'un lac (rayon au carré, m^2),
   utilisé seulement si le remplissage de proche en proche (flood fill) ne trouve
   pas d'eau sous le repère (repère mal placé). Le masque d'eau, lui, épouse la vraie
   forme du plan d'eau, quelle que soit sa taille. */
constexpr float LAKE_FALLBACK_R2 = 120.0f * 120.0f;

/* Nombre d'espèces de l'atlas trees_atlas.png (sapin, feuillu, mélèze, pin). Doit
   correspondre à ATLAS_COUNT dans vegetation.vert. */
constexpr int NUM_SPECIES = 4;
static_assert(NUM_SPECIES == 4, "trees_atlas.png et ATLAS_COUNT (vegetation.vert) : 4 espèces");

/* Garde-fou : plafond du nombre d'arbres, pour éviter une explosion mémoire si
   la grille est réglée très fine (6 M x 20 octets ~ 120 Mo de tampon d'instances).
   Le balayage étant nord -> sud, atteindre ce plafond tronquerait le semis au sud :
   on avertit alors et on invite à agrandir l'espacement. */
constexpr std::size_t MAX_TREES = 6'000'000;

/* Budget d'arbres : au-delà, on éclaircit uniformément le semis (chaque arbre gardé
   avec la même probabilité, de façon déterministe) pour tenir la charge sur les
   grandes cartes très boisées, où les billboards croisés génèrent beaucoup de
   surdessin (Bordeaux : ~5 M d'arbres). Ossau (~1,1 M) reste sous le budget, inchangé.
   Réglable par ARTOUSTE_TREE_MAX. */
constexpr std::size_t TARGET_TREES = 1'600'000;

/* Générateur pseudo-aléatoire déterministe (LCG) à partir d'une graine entière :
   même semis d'une exécution à l'autre (pas de scintillement, résultats stables). */
inline std::uint32_t hashU32(std::uint32_t x) {
    x ^= 0x9e3779b9u;
    x = x * 1664525u + 1013904223u;
    x ^= x >> 16;
    x = x * 2654435761u;
    return x;
}

/* Réel dans [0,1) tiré d'une graine. */
inline float unitOf(std::uint32_t seed) {
    return static_cast<float>(hashU32(seed) >> 8) / 16777216.0f;
}

/* Signature de couleur de la forêt dans l'orthophoto (canaux normalisés 0..1) :
   vert dominant, ni trop sombre (ombres), ni trop clair (prairies, champs), plus vert
   que bleu. Seuils élargis après diagnostic (carte de couverture sur Ossau) : le
   filtre d'origine (g >= r*1.02, g < 0.44) laissait des trous dans de vraies forêts,
   surtout sur les versants ensoleillés (verts jaunâtres) ; on inclut donc ces verts
   plus jaunes et un peu plus clairs, tout en laissant la roche, l'éboulis et l'eau
   non couverts. */
inline bool looksLikeForest(float r, float g, float b) {
    /* Zone claire ou blanche (grève, gravier, roche/neige au soleil, chemin) : pas
       d'arbre. On l'écarte sur la luminance, au-dessus du vert sombre de la forêt. */
    const float lum = 0.299f * r + 0.587f * g + 0.114f * b;
    if (lum > 0.46f) {
        return false;
    }
    return g > 0.12f && g < 0.48f && g >= r * 0.97f && g >= b * 1.08f && b < 0.48f;
}

/* Sol minéral vu dans l'orthophoto : bitume, béton, gravier, sable, toiture. Sert
   de VETO à l'intérieur d'une forêt cartographiée, pour rattraper ce que le
   contour ne sait pas : une coupe rase récente, un pare-feu, une piste forestière,
   un bâtiment isolé sous couvert. Mesuré sur cote-landes, la couleur ne sait PAS
   séparer forêt et pelouse (les deux sont vertes, excès de vert médian 0,037 et
   0,033) ; elle sépare en revanche nettement le végétal du minéral. Le veto reste
   donc lâche exprès : il n'écarte que le franchement clair et le franchement gris,
   soit environ 1 % des pixels de forêt, et épargne l'ombre profonde d'un versant
   nord (où l'excès de vert s'effondre sans que la forêt disparaisse). */
inline bool looksMineral(float r, float g, float b) {
    const float lum  = 0.299f * r + 0.587f * g + 0.114f * b;
    const float vert = g - 0.5f * (r + b);  /* excès de vert */
    return lum > 0.58f || (vert < 0.0f && lum > 0.25f);
}

} /* namespace artouste::render */
