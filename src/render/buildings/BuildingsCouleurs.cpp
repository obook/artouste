/*
 * BuildingsCouleurs.cpp
 * Couleur des murs et des toits (voir BuildingsCouleurs.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/buildings/BuildingsCouleurs.hpp"

#include "render/Terrain.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cstdio>

namespace artouste::render {

const vec3 WALL_COLOR{0.86f, 0.84f, 0.80f};

namespace {

/* Palette de repli, quand l'orthophoto est absente ou l'emprise trop petite :
   la tuile terre cuite domine (3 entrées sur 6). */
const vec3 ROOF_PALETTE[] = {
    {0.62f, 0.32f, 0.24f}, /* tuile terre cuite (dominante) */
    {0.62f, 0.32f, 0.24f},
    {0.62f, 0.32f, 0.24f},
    {0.70f, 0.38f, 0.27f}, /* tuile plus chaude / neuve */
    {0.48f, 0.26f, 0.21f}, /* tuile patinée, plus sombre */
    {0.44f, 0.42f, 0.44f}, /* ardoise grise */
};

/* Le toit reçoit un éclairage plus sombre que le sol (building.frag contre
   terrain.frag) : sans ce gain il se lirait comme une tache sombre sur sa
   propre empreinte dans l'ortho. */
constexpr float ROOF_GAIN = 1.10f;
/* Une cour à l'ombre ne doit pas donner un toit noir, ni une verrière
   surexposée un toit blanc. */
constexpr float ROOF_MIN = 0.18f;
constexpr float ROOF_MAX = 0.85f;
/* Saturation visée. Paris est gris (0,024), Dax est franc (0,052) : on étale
   d'autant plus que l'image est terne, sans jamais comprimer une image déjà
   franche ni fabriquer du contraste au-delà du plafond. */
constexpr float ROOF_TARGET_SAT   = 0.05f;
constexpr float ROOF_CONTRAST_MAX = 1.35f;
/* Nuance chaude ou froide par bâtiment : l'ortho ne peut pas la donner, elle
   est quasi achromatique, et sans elle une ville de zinc vire au gris. */
constexpr float ROOF_TINT = 0.06f;
/* En deçà de deux pixels de côté, la teinte lue n'est plus une toiture. */
constexpr float ROOF_ORTHO_MIN_PX = 2.0f;

/* Signature couleur de l'eau, et altitude au-delà de laquelle un bâtiment
   n'est jamais sur l'eau. La seule altitude ne suffit pas : la carte de relief
   est grossière et rate les chenaux étroits des ports. */
constexpr float WATER_ALT_M     = 4.0f;
constexpr float WATER_RED       = 0.30f;
constexpr float WATER_BLUE_BIAS = 0.08f;

/* Teinte stable tirée de la palette, décorrélée du jitter de luminosité. */
[[nodiscard]] const vec3& pickRoof(std::uint32_t seed) {
    seed ^= 0x9e3779b9u;
    seed = seed * 2654435761u + 2246822519u;
    const std::size_t n = sizeof(ROOF_PALETTE) / sizeof(ROOF_PALETTE[0]);
    return ROOF_PALETTE[(seed >> 16) % n];
}

} /* namespace */

float jitter(std::uint32_t seed, float amp) {
    seed             = seed * 1664525u + 1013904223u;               /* LCG classique */
    const float unit = static_cast<float>(seed >> 8) / 16777216.0f; /* [0,1) */
    return 1.0f + (unit * 2.0f - 1.0f) * amp;
}

Ortho::Ortho(const std::filesystem::path& image, const Terrain& terrain) {
    m_halfW = terrain.halfWidth();
    m_halfH = terrain.halfHeight();
    m_origX = terrain.originX(); /* centre de l'emprise (0 sauf carte recadrée) */
    m_origZ = terrain.originZ();

    int canaux = 0;
    stbi_set_flip_vertically_on_load(0); /* rangée 0 = nord, comme le relief */
    m_pixels = stbi_load(image.string().c_str(), &m_w, &m_h, &canaux, 3);
    if (m_pixels == nullptr) {
        return;
    }
    m_pixelX = 2.0f * m_halfW / static_cast<float>(m_w);
    m_pixelZ = 2.0f * m_halfH / static_cast<float>(m_h);

    /* Luminosité et saturation moyennes, sur un pixel sur huit dans chaque
       direction : inutile de lire les treize millions pour une teinte
       d'ensemble. */
    constexpr int ORTHO_STEP = 8;
    double        sommeLum = 0.0, sommeSat = 0.0;
    std::size_t   n = 0;
    for (int y = 0; y < m_h; y += ORTHO_STEP) {
        for (int x = 0; x < m_w; x += ORTHO_STEP) {
            const unsigned char* p =
                m_pixels + (static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) +
                            static_cast<std::size_t>(x)) *
                               3;
            sommeLum += (0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]) / 255.0;
            const int haut = std::max({p[0], p[1], p[2]});
            const int bas  = std::min({p[0], p[1], p[2]});
            sommeSat += (haut - bas) / 255.0;
            ++n;
        }
    }
    if (n > 0) {
        m_luminosite            = static_cast<float>(sommeLum / static_cast<double>(n));
        const float saturation  = static_cast<float>(sommeSat / static_cast<double>(n));
        m_contraste = std::clamp(ROOF_TARGET_SAT / std::max(saturation, 0.005f), 1.0f,
                                 ROOF_CONTRAST_MAX);
        std::printf("[Buildings] ortho : luminosité moyenne %.3f, saturation moyenne %.3f "
                    "-> contraste des toitures x%.2f\n",
                    static_cast<double>(m_luminosite),
                    static_cast<double>(saturation),
                    static_cast<double>(m_contraste));
    }
}

Ortho::~Ortho() {
    if (m_pixels != nullptr) {
        stbi_image_free(m_pixels);
    }
}

const unsigned char* Ortho::pixel(float x, float z) const {
    if (m_pixels == nullptr) {
        return nullptr;
    }
    const float u  = (x - m_origX + m_halfW) / (2.0f * m_halfW); /* 0 = ouest, 1 = est */
    const float v  = (z - m_origZ + m_halfH) / (2.0f * m_halfH); /* 0 = nord,  1 = sud */
    const int   ox = std::clamp(static_cast<int>(u * static_cast<float>(m_w - 1)), 0, m_w - 1);
    const int   oy = std::clamp(static_cast<int>(v * static_cast<float>(m_h - 1)), 0, m_h - 1);
    return m_pixels + (static_cast<std::size_t>(oy) * static_cast<std::size_t>(m_w) +
                       static_cast<std::size_t>(ox)) *
                          3;
}

vec3 Ortho::couleurToit(float         cx,
                        float         cz,
                        float         largeurM,
                        float         profondeurM,
                        std::uint32_t seed) const {
    const bool lisible = m_pixels != nullptr && largeurM >= ROOF_ORTHO_MIN_PX * m_pixelX &&
                         profondeurM >= ROOF_ORTHO_MIN_PX * m_pixelZ;

    vec3 toit;
    if (lisible) {
        const unsigned char* p     = pixel(cx, cz);
        const auto           canal = [this](unsigned char c) {
            const float v = static_cast<float>(c) / 255.0f * ROOF_GAIN;
            return std::clamp(m_luminosite + (v - m_luminosite) * m_contraste, ROOF_MIN,
                                        ROOF_MAX);
        };
        toit = vec3{canal(p[0]), canal(p[1]), canal(p[2])};
    } else {
        toit = pickRoof(seed);
    }

    /* La nuance de luminosité est gardée dans les deux cas : un pâté
       d'immeubles voisins tombe souvent sur le même pixel d'ortho, et sans elle
       les toits s'aplatiraient en une nappe unie. */
    toit *= jitter(seed, 0.12f);

    /* Rouge et bleu tirés en sens inverse : la teinte bascule sans que la
       luminosité bouge. */
    const float chaud = jitter(seed * 40503u + 3u, ROOF_TINT);
    toit.x *= chaud;
    toit.z /= chaud;
    return toit;
}

bool Ortho::surLeau(float x, float z, float altitudeSol) const {
    if (altitudeSol > WATER_ALT_M) {
        return false; /* ville en hauteur : jamais sur l'eau */
    }
    const unsigned char* p = pixel(x, z);
    if (p == nullptr) {
        return false;
    }
    const float rouge = p[0] / 255.0f;
    const float bleu  = p[2] / 255.0f;
    return rouge <= WATER_RED && (bleu - rouge) >= WATER_BLUE_BIAS;
}

} /* namespace artouste::render */
