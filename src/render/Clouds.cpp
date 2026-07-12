/*
 * Clouds.cpp
 * Sème une couche de cumulus en bouffées (billboards) au-dessus du relief, puis les
 * dessine par instanciation GPU. Chaque nuage est un amas de bouffées réparties dans
 * un ellipsoïde à base plate (plus étroit vers le haut, façon cumulus) ; l'ombrage
 * clair-en-haut / sombre-en-bas est fait par le shader. Les bouffées, transparentes,
 * sont retriées de l'arrière vers l'avant à chaque image pour un mélange alpha correct.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/Clouds.hpp"

#include "render/Terrain.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace artouste::render {

namespace {

/* Espacement de la grille de cumulus (m) et probabilité qu'une maille porte un nuage :
   ciel de beau temps, épars. */
constexpr float CLOUD_SPACING = 2600.0f;
constexpr float CLOUD_PROB    = 0.28f;

/* Marge horizontale au-delà de l'emprise du terrain (le ciel déborde du relief). */
constexpr float SKY_MARGIN = 1.4f;

/* Au-delà de cette distance, on ne dessine pas la bouffée (la brume la masque de
   toute façon) : économise le remplissage. */
constexpr float VIEW_DIST    = 48000.0f;
constexpr float VIEW_DIST2   = VIEW_DIST * VIEW_DIST;

constexpr std::size_t MAX_PUFFS = 8000;

/* Générateur pseudo-aléatoire déterministe (LCG haché) : ciel identique d'une
   exécution à l'autre. */
std::uint32_t hashU32(std::uint32_t x) {
    x ^= 0x9e3779b9u;
    x = x * 1664525u + 1013904223u;
    x ^= x >> 16;
    x = x * 2654435761u;
    return x;
}
float unitOf(std::uint32_t s) {
    return static_cast<float>(hashU32(s) >> 8) / 16777216.0f;
}

}  /* namespace */

Clouds::Clouds(const Terrain& terrain, const std::filesystem::path& puffPath) {
    m_puff = Texture(puffPath);
    if (!m_puff.valid()) {
        std::fprintf(stderr, "[Clouds] bouffée absente (%s) : nuages ignorés.\n",
                     puffPath.string().c_str());
        return;
    }

    /* Base des cumulus : au-dessus des plus hauts sommets pour rester dans le ciel,
       mais jamais sous ~1000 m (cartes de plaine). */
    const float base0 = std::max(1000.0f, terrain.maxElevation() * 0.85f);
    const float halfW = terrain.halfWidth() * SKY_MARGIN;
    const float halfH = terrain.halfHeight() * SKY_MARGIN;

    const int cols = std::max(1, static_cast<int>((2.0f * halfW) / CLOUD_SPACING));
    const int rows = std::max(1, static_cast<int>((2.0f * halfH) / CLOUD_SPACING));

    for (int r = 0; r < rows && m_puffs.size() < MAX_PUFFS; ++r) {
        for (int c = 0; c < cols && m_puffs.size() < MAX_PUFFS; ++c) {
            const std::uint32_t seed = hashU32(static_cast<std::uint32_t>(r) * 40499u
                                               ^ static_cast<std::uint32_t>(c) * 86969u);
            if (unitOf(seed) > CLOUD_PROB) {
                continue;  /* maille sans nuage : ciel épars */
            }
            /* Centre du nuage, perturbé dans la maille. */
            const float cx = -halfW + (static_cast<float>(c) + unitOf(seed ^ 0x11u)) * CLOUD_SPACING;
            const float cz = -halfH + (static_cast<float>(r) + unitOf(seed ^ 0x22u)) * CLOUD_SPACING;
            const float base = base0 + (unitOf(seed ^ 0x33u) - 0.5f) * 300.0f;

            /* Dimensions du cumulus : nettement plus large que haut (aspect aplati de
               beau temps), base plate. */
            const float wx = 450.0f + unitOf(seed ^ 0x44u) * 550.0f;
            const float wz = 450.0f + unitOf(seed ^ 0x55u) * 550.0f;
            const float ht = 220.0f + unitOf(seed ^ 0x66u) * 260.0f;
            const int   n  = 16 + static_cast<int>(unitOf(seed ^ 0x77u) * 14.0f);

            for (int p = 0; p < n; ++p) {
                const std::uint32_t ps = hashU32(seed ^ (static_cast<std::uint32_t>(p) * 2654435761u));
                /* Hauteur biaisée vers le bas (t = u^2) : beaucoup de bouffées à la
                   base, étalées sur toute la largeur (fond plat), quelques-unes qui
                   montent au centre en se resserrant (sommet bombé, façon chou-fleur). */
                const float u   = unitOf(ps);
                const float t   = u * u;
                const float ang = unitOf(ps ^ 0xa1u) * 6.2831853f;
                const float rad = std::sqrt(unitOf(ps ^ 0xb2u));
                const float shrink = 1.0f - 0.7f * t;      /* se resserre franchement en haut */
                const float px = cx + std::cos(ang) * rad * wx * shrink;
                const float pz = cz + std::sin(ang) * rad * wz * shrink;
                const float py = base + t * ht;
                /* Bouffées plus grosses en bas (masse du nuage), plus petites au sommet. */
                const float sz = (360.0f - 120.0f * t) + unitOf(ps ^ 0xc3u) * 200.0f;
                m_puffs.push_back(Puff{vec3{px, py, pz}, sz, t});
                if (m_puffs.size() >= MAX_PUFFS) {
                    break;
                }
            }
        }
    }

    if (m_puffs.empty()) {
        return;
    }
    m_order.reserve(m_puffs.size());
    m_instances.reserve(m_puffs.size() * 5);

    /* Quad de base (deux triangles) : coin (x,y dans [-0.5,0.5]) + UV. */
    const float quad[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
        0.5f, -0.5f, 1.0f, 0.0f,
        0.5f, 0.5f, 1.0f, 1.0f,
        -0.5f, 0.5f, 0.0f, 1.0f,
    };
    const unsigned int indices[] = {0, 1, 2, 0, 2, 3};

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    /* Tampon d'instances (dynamique : réécrit trié à chaque image). Attributs
       2 (centre), 3 (taille), 4 (hauteur relative). Cinq flottants par instance. */
    constexpr GLsizei istride = 5 * sizeof(float);
    glGenBuffers(1, &m_instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_puffs.size() * 5 * sizeof(float)), nullptr,
                 GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, istride, reinterpret_cast<void*>(0));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, istride, reinterpret_cast<void*>(3 * sizeof(float)));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, istride, reinterpret_cast<void*>(4 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    std::printf("[Clouds] %zu bouffées semées.\n", m_puffs.size());
}

Clouds::~Clouds() {
    release();
}

void Clouds::release() noexcept {
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
}

void Clouds::draw(const vec3& camWorld) {
    if (m_puffs.empty()) {
        return;
    }

    /* Tri arrière -> avant : distance au carré à la caméra, décroissante. On écarte
       au passage les bouffées trop lointaines (masquées par la brume). */
    m_order.clear();
    for (std::size_t i = 0; i < m_puffs.size(); ++i) {
        const vec3  d  = m_puffs[i].pos - camWorld;
        const float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
        if (d2 < VIEW_DIST2) {
            m_order.emplace_back(d2, i);
        }
    }
    if (m_order.empty()) {
        return;
    }
    std::sort(m_order.begin(), m_order.end(),
              [](const std::pair<float, std::size_t>& a, const std::pair<float, std::size_t>& b) {
                  return a.first > b.first;  /* le plus loin d'abord */
              });

    m_instances.clear();
    for (const auto& o : m_order) {
        const Puff& p = m_puffs[o.second];
        m_instances.push_back(p.pos.x);
        m_instances.push_back(p.pos.y);
        m_instances.push_back(p.pos.z);
        m_instances.push_back(p.size);
        m_instances.push_back(p.vfrac);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_instances.size() * sizeof(float)), m_instances.data(),
                 GL_DYNAMIC_DRAW);

    m_puff.bind(0);
    glBindVertexArray(m_vao);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(m_order.size()));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

}  /* namespace artouste::render */
