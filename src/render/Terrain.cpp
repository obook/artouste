/*
 * Terrain.cpp
 * Construit le maillage du terrain à partir de la carte d'altitude et drape
 * l'orthophoto. Les altitudes sont aussi gardées en mémoire, interrogées à
 * l'exécution par heightAt() (voir TerrainQuery.cpp). Le chargement des
 * fichiers annexes et l'aplanissement du relief sont dans TerrainSetup.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"

#include "render/terrain/TerrainInterne.hpp"

#include "render/TextureCache.hpp"

#include "render/Primitives.hpp"

#include <glad/glad.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <utility>
#include <sstream>
#include <string>

namespace artouste::render {

Terrain::Terrain(const std::filesystem::path& dir,
                 bc7::Progression             progression,
                 int                          fenetreDetailPx,
                 int                          sommetsMax,
                 std::filesystem::path        racineTuiles,
                 bool                         fenetreRelief)
    : m_progression(std::move(progression)) {
    const std::filesystem::path meta = dir / "terrain.txt";
    const std::filesystem::path height = dir / "heightmap.png";
    const std::filesystem::path ortho = dir / "ortho.jpg";

    /* Lieux remarquables, hélipads et balises HAPI du terrain (facultatifs : absent
       = aucun). */
    loadPlaces(dir / "landmarks.txt", m_landmarks, "lieu(x) remarquable(s)");
    loadPlaces(dir / "helipads.txt", m_helipads, "hélipad(s)");
    loadHapiUnits(dir / "hapi.txt", m_hapiUnits);
    loadMonuments(dir / "monuments.txt", m_monuments);

    if (!readMetadata(meta,
                      m_cols,
                      m_rows,
                      m_widthM,
                      m_heightM,
                      m_elevMin,
                      m_elevMax,
                      m_drawSea,
                      m_hasStart,
                      m_startX,
                      m_startZ,
                      m_startHeadingDeg,
                      m_hasGeo,
                      m_lonMin,
                      m_lonMax,
                      m_latMin,
                      m_latMax,
                      m_originX,
                      m_originZ)) {
        std::fprintf(stderr,
                     "[Terrain] calage absent (%s), repli sur un sol plat.\n",
                     meta.string().c_str());
        buildFlatFallback();
        return;
    }

    /* Lecture de la carte d'altitude en 16 bits, sans retournement vertical :
       on garde la rangée 0 au nord, comme l'a écrite l'outil de préparation. */
    stbi_set_flip_vertically_on_load(0);
    int w = 0, h = 0, channels = 0;
    unsigned short* pixels = stbi_load_16(height.string().c_str(), &w, &h, &channels, 1);
    if (pixels == nullptr || w != m_cols || h != m_rows) {
        std::fprintf(stderr,
                     "[Terrain] heightmap illisible ou de taille inattendue (%s).\n",
                     height.string().c_str());
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        buildFlatFallback();
        return;
    }

    /* Reconstitution des altitudes en mètres à partir des niveaux de gris. */
    const float span = m_elevMax - m_elevMin;
    m_heights.resize(static_cast<std::size_t>(m_cols) * static_cast<std::size_t>(m_rows));
    for (std::size_t k = 0; k < m_heights.size(); ++k) {
        m_heights[k] = m_elevMin + (static_cast<float>(pixels[k]) / 65535.0f) * span;
    }
    stbi_image_free(pixels);

    /* Point de départ : on aplanit le relief sous le spawn, pour que le sol et
       l'appareil posé s'accordent à une même hauteur (sinon, sur une maille en
       pente, l'appareil s'enfonce ou se pose en travers). À faire avant de
       construire le maillage, qui en hérite. Les hélipads du terrain, eux, ne
       déforment plus le relief : chacun est une petite plate-forme portée par
       heightAt (voir buildPadPlatforms), le disque et sa jupe habillant le
       surplomb éventuel. */
    flattenPads();
    buildPadPlatforms();

    construireMaillage(sommetsMax);

    /* Seule texture compressée du moteur : c'est la plus grosse de loin (des
       dizaines de mégapixels sur une carte fine), elle est opaque, et son
       budget mémoire est ce qui plafonne la finesse au sol qu'on peut se
       permettre par carte. */
    m_ortho = cache::chargerOrthophoto(ortho, m_progression);
    m_textured = m_ortho.valid();
    if (!m_textured) {
        std::fprintf(stderr,
                     "[Terrain] orthophoto absente (%s), relief sans texture.\n",
                     ortho.string().c_str());
    } else {
        std::printf("[Terrain] terrain chargé : %.0f x %.0f m, altitude max %.0f m.\n",
                    static_cast<double>(m_widthM),
                    static_cast<double>(m_heightM),
                    static_cast<double>(m_elevMax));
        ouvrirDetail(dir, fenetreDetailPx, racineTuiles);
        /* Éteinte, la fenêtre ne coûte rien du tout : ses tuiles ne sont pas
           lues. Voir la clé "relief_fenetre" de config.txt. */
        if (fenetreRelief) {
            ouvrirRelief(dir, racineTuiles);
        } else {
            std::printf("[relief] fenêtre de relief fin ÉTEINTE "
                        "(clé relief_fenetre ; ARTOUSTE_RELIEF l'allume).\n");
        }
    }
}

} /* namespace artouste::render */
