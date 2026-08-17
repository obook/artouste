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

namespace {

/*
 * Lit le fichier de calage terrain.txt (lignes "clé valeur", # = commentaire)
 * et range les valeurs attendues. Renvoie faux si une clé manque.
 */
bool readMetadata(const std::filesystem::path& path,
                  int& cols,
                  int& rows,
                  float& widthM,
                  float& heightM,
                  float& elevMin,
                  float& elevMax,
                  bool& drawSea,
                  bool& hasStart,
                  float& startX,
                  float& startZ,
                  float& startHeadingDeg,
                  bool& hasGeo,
                  float& lonMin,
                  float& lonMax,
                  float& latMin,
                  float& latMax,
                  float& originX,
                  float& originZ) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    originX = 0.0f; /* facultatifs : 0 = emprise centrée sur l'origine du monde */
    originZ = 0.0f;
    bool hasCols = false, hasRows = false, hasW = false, hasH = false, hasMin = false,
         hasMax = false;
    bool hasStartX = false, hasStartZ = false;
    bool hasLonMin = false, hasLonMax = false, hasLatMin = false, hasLatMax = false;
    std::string key;
    while (file >> key) {
        if (!key.empty() && key[0] == '#') {
            std::getline(file, key); /* on jette le reste de la ligne de commentaire */
            continue;
        }
        if (key == "cols") {
            file >> cols, hasCols = true;
        } else if (key == "rows") {
            file >> rows, hasRows = true;
        } else if (key == "width_m") {
            file >> widthM, hasW = true;
        } else if (key == "height_m") {
            file >> heightM, hasH = true;
        } else if (key == "elev_min") {
            file >> elevMin, hasMin = true;
        } else if (key == "elev_max") {
            file >> elevMax, hasMax = true;
        } else if (key == "sea") { /* 0 = pas de plan de mer (terrain de montagne) */
            int v = 1;
            file >> v;
            drawSea = (v != 0);
        } else if (key == "start_x") {
            file >> startX, hasStartX = true;
        } else if (key == "start_z") {
            file >> startZ, hasStartZ = true;
        } else if (key == "start_heading") { /* cap initial (deg boussole), facultatif */
            file >> startHeadingDeg;
        } else if (key == "origin_x") { /* décalage d'origine (carte recadrée), facultatif */
            file >> originX;
        } else if (key == "origin_z") {
            file >> originZ;
        } else if (key == "lon_min") {
            file >> lonMin, hasLonMin = true;
        } else if (key == "lon_max") {
            file >> lonMax, hasLonMax = true;
        } else if (key == "lat_min") {
            file >> latMin, hasLatMin = true;
        } else if (key == "lat_max") {
            file >> latMax, hasLatMax = true;
        } else {
            std::getline(file, key); /* clé ignorée : on saute sa valeur */
        }
    }
    hasStart = hasStartX && hasStartZ;
    hasGeo = hasLonMin && hasLonMax && hasLatMin && hasLatMax;
    return hasCols && hasRows && hasW && hasH && hasMin && hasMax;
}

} /* namespace */

Terrain::Terrain(const std::filesystem::path& dir,
                 bc7::Progression             progression,
                 int                          fenetreDetailPx,
                 int                          sommetsMax,
                 std::filesystem::path        racineTuiles)
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

    /* --- Construction du maillage du relief ---------------------------------- */
    const float halfW = 0.5f * m_widthM;
    const float halfH = 0.5f * m_heightM;
    const float dx = m_widthM / static_cast<float>(m_cols - 1);  /* pas est-ouest (m) */
    const float dz = m_heightM / static_cast<float>(m_rows - 1); /* pas nord-sud (m) */

    /* Points de grille RETENUS pour le maillage. La carte d'altitude peut être
       plus fine que ce qu'on veut dessiner : doubler sa finesse quadruple le
       nombre de triangles, ce qu'un GPU intégré ne suit pas, alors que les
       altitudes elles-mêmes restent utiles en entier pour heightAt (poser,
       plates-formes, collision). On échantillonne donc le relief pour le dessin
       sans rien jeter pour la physique.

       Le dernier point de chaque axe est toujours retenu, quitte à raccourcir la
       dernière maille : les positions se déduisent de l'indice réel, la
       géométrie reste donc exacte jusqu'au bord de l'emprise. */
    const auto retenus = [](int nb, int pas) {
        std::vector<int> pris;
        pris.reserve(static_cast<std::size_t>(nb / pas + 2));
        for (int k = 0; k < nb - 1; k += pas) {
            pris.push_back(k);
        }
        pris.push_back(nb - 1);
        return pris;
    };
    /* Pas d'échantillonnage : le plus petit qui tienne dans le budget de
       sommets. Un pas de s divise leur nombre par s au carré. */
    int pasMaillage = 1;
    if (sommetsMax > 0) {
        const double total = static_cast<double>(m_cols) * static_cast<double>(m_rows);
        pasMaillage = std::max(1, static_cast<int>(std::ceil(
                                      std::sqrt(total / static_cast<double>(sommetsMax)))));
    }
    const std::vector<int> colonnes    = retenus(m_cols, pasMaillage);
    const std::vector<int> rangees     = retenus(m_rows, pasMaillage);
    if (pasMaillage > 1) {
        std::printf("[Terrain] maillage allégé : 1 point sur %d, %zu x %zu sommets "
                    "(maille %.0f m au lieu de %.0f m).\n",
                    pasMaillage,
                    colonnes.size(),
                    rangees.size(),
                    static_cast<double>(dx * static_cast<float>(pasMaillage)),
                    static_cast<double>(dx));
    }

    primitives::MeshData data;
    data.vertices.reserve(colonnes.size() * rangees.size());
    data.indices.reserve((colonnes.size() - 1) * (rangees.size() - 1) * 6);

    /* Indice linéaire d'un point (colonne i, rangée j) dans la grille. */
    const auto idx = [cols = m_cols](int i, int j) -> std::size_t {
        return static_cast<std::size_t>(j) * static_cast<std::size_t>(cols) +
               static_cast<std::size_t>(i);
    };

    const vec3 white{1.0f, 1.0f, 1.0f}; /* la couleur vient de la texture */

    /* Pas du gradient des normales : environ 35 m de part et d'autre, quelle
       que soit la finesse de la grille. Au pas d'une seule maille fine
       (17,5 m), chaque micro-facette accrochait la lumière et les versants
       lointains scintillaient en quadrillage régulier ; on lisse l'ÉCLAIRAGE
       sans rien enlever au relief lui-même. */
    const int step = std::max(1, static_cast<int>(std::lround(35.0f / dx)));

    for (const int j : rangees) {
        for (const int i : colonnes) {
            const float x = m_originX - halfW + static_cast<float>(i) * dx;
            const float z =
                m_originZ - halfH + static_cast<float>(j) * dz; /* rangée 0 = nord (Z min) */
            const float y = m_heights[idx(i, j)];

            /* Normale par différences finies sur le relief (voisins bornés au bord). */
            const int iL = std::max(0, i - step);
            const int iR = std::min(m_cols - 1, i + step);
            const int jU = std::max(0, j - step);
            const int jD = std::min(m_rows - 1, j + step);
            const float hL = m_heights[idx(iL, j)];
            const float hR = m_heights[idx(iR, j)];
            const float hU = m_heights[idx(i, jU)];
            const float hD = m_heights[idx(i, jD)];
            const float dydx = (hR - hL) / (static_cast<float>(iR - iL) * dx);
            const float dydz = (hD - hU) / (static_cast<float>(jD - jU) * dz);
            const vec3 normal = glm::normalize(vec3{-dydx, 1.0f, -dydz});

            Vertex v;
            v.position = vec3{x, y, z};
            v.normal = normal;
            v.color = white;
            v.uv = vec2{static_cast<float>(i) / static_cast<float>(m_cols - 1),
                        1.0f - static_cast<float>(j) / static_cast<float>(m_rows - 1)};
            data.vertices.push_back(v);
        }
    }

    /* Les indices portent sur les sommets RETENUS, pas sur la grille d'origine. */
    const auto largeurMaillage = static_cast<unsigned int>(colonnes.size());
    for (unsigned int j = 0; j + 1 < rangees.size(); ++j) {
        for (unsigned int i = 0; i + 1 < largeurMaillage; ++i) {
            const unsigned int a = j * largeurMaillage + i;
            const unsigned int b = a + 1;
            const unsigned int c = a + largeurMaillage;
            const unsigned int d = c + 1;
            data.indices.insert(data.indices.end(), {a, c, b, b, c, d});
        }
    }

    m_mesh = Mesh(data.vertices, data.indices);

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
        ouvrirRelief(dir, racineTuiles);
    }
}

void Terrain::ouvrirDetail(const std::filesystem::path& dir, int fenetrePx,
                           const std::filesystem::path& racineTuiles) {
    if (fenetrePx <= 0) {
        return;  /* détail fin refusé par la configuration */
    }
    const std::filesystem::path candidat = tuiles::cheminJeuDeTuiles(dir, racineTuiles);
    if (!candidat.empty()) {
        std::vector<tuiles::Pyramide> niveaux = tuiles::ouvrirNiveaux(candidat);

        /* Des tuiles pas plus fines que l'orthophoto déjà chargée n'apporteraient
           rien et coûteraient de la mémoire vidéo : c'est le cas d'un jeu de
           tuiles produit à la finesse de la source pour une carte dont
           l'orthophoto d'ensemble est déjà fine. */
        const float finesseOrtho = orthoMetersPerPixel();
        if (finesseOrtho > 0.0f) {
            const std::size_t avant = niveaux.size();
            std::erase_if(niveaux, [finesseOrtho](const tuiles::Pyramide& p) {
                return !tuiles::niveauUtile(p.calage().mParPixel, finesseOrtho);
            });
            if (niveaux.empty()) {
                std::printf("[tuiles] %s : aucun niveau plus fin que l'orthophoto "
                            "(%.2f m/px), fenêtre inutile.\n",
                            candidat.string().c_str(),
                            static_cast<double>(finesseOrtho));
                return;
            }
            if (niveaux.size() != avant) {
                std::printf("[tuiles] %s : %zu niveau(x) écarté(s), pas plus fin(s) que "
                            "l'orthophoto.\n",
                            candidat.string().c_str(),
                            avant - niveaux.size());
            }
        }

        /* Deux niveaux au plus, les DEUX PLUS FINS : au-delà, l'orthophoto
           d'ensemble couvre déjà le lointain, et chaque fenêtre supplémentaire
           coûterait sa mémoire vidéo pour une distance où l'oeil ne distingue
           plus rien. Ils sont classés du plus large au plus fin, on garde donc
           la fin de la liste. */
        if (niveaux.size() > 2) {
            niveaux.erase(niveaux.begin(),
                          niveaux.end() - 2);
        }

        /* La fenêtre serrée est deux fois plus petite que la large : elle ne sert
           qu'au ras du sol, où son rayon suffit largement, et la seconde moitié
           de sa mémoire vidéo serait dépensée pour du terrain que le niveau
           large couvre déjà correctement. */
        m_detail = std::make_unique<tuiles::Fenetre>(std::move(niveaux.front()), fenetrePx);
        if (!m_detail->active()) {
            m_detail.reset();
            return;
        }
        if (niveaux.size() > 1) {
            auto serree =
                std::make_unique<tuiles::Fenetre>(std::move(niveaux.back()),
                                                  std::max(1024, fenetrePx / 2));
            if (serree->active()) {
                m_detailFin = std::move(serree);
            }
        }
        return;
    }
}

void Terrain::ouvrirRelief(const std::filesystem::path& dir,
                           const std::filesystem::path& racineTuiles) {
    const std::filesystem::path candidat = relief::cheminJeuDeRelief(dir, racineTuiles);
    if (candidat.empty() || m_heights.empty()) {
        return;
    }

    /* Relief d'ensemble en texture, tel qu'il est après aplanissement du départ :
       la fenêtre s'y raccorde au bord, et une tuile absente y ramène. */
    glGenTextures(1, &m_carteRelief);
    glBindTexture(GL_TEXTURE_2D, m_carteRelief);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_cols, m_rows, 0, GL_RED, GL_FLOAT,
                 m_heights.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_relief = relief::FenetreRelief::ouvrir(
        candidat,
        [this](float x0, float z0, float pasX, float pasZ, int cote, float* hauteurs,
               bool aDonnee) {
            corrigerTuileRelief(x0, z0, pasX, pasZ, cote, hauteurs, aDonnee);
        },
        relief::COTE_GRILLE_POINTS);
    if (!m_relief) {
        glDeleteTextures(1, &m_carteRelief);
        m_carteRelief = 0;
    }
}

void Terrain::corrigerTuileRelief(float x0, float z0, float pasX, float pasZ, int cote,
                                  float* hauteurs, bool aDonnee) const noexcept {
    /* Le départ est aplani dans le maillage d'ensemble (voir flattenPads) : le
       relief fin doit y revenir, sinon l'appareil naîtrait sur une bosse. */
    constexpr float PLAT_M  = 40.0f;
    constexpr float FONDU_M = 40.0f;
    const float     demiX   = 0.5f * static_cast<float>(cote) * pasX;
    const float     demiZ   = 0.5f * static_cast<float>(cote) * pasZ;
    const bool      proche  = m_hasStart &&
                        std::fabs(x0 + demiX - m_startX) < demiX + PLAT_M + FONDU_M &&
                        std::fabs(z0 + demiZ - m_startZ) < demiZ + PLAT_M + FONDU_M;
    if (aDonnee && !proche) {
        return;
    }

    for (int j = 0; j < cote; ++j) {
        const float z = z0 + static_cast<float>(j) * pasZ;
        for (int i = 0; i < cote; ++i) {
            const float       x = x0 + static_cast<float>(i) * pasX;
            const std::size_t k = static_cast<std::size_t>(j) * static_cast<std::size_t>(cote) +
                                  static_cast<std::size_t>(i);
            if (!aDonnee) {
                hauteurs[k] = heightCoarse(x, z);
                continue;
            }
            const float d = std::sqrt((x - m_startX) * (x - m_startX) +
                                      (z - m_startZ) * (z - m_startZ));
            if (d >= PLAT_M + FONDU_M) {
                continue;
            }
            const float t = std::clamp((d - PLAT_M) / FONDU_M, 0.0f, 1.0f);
            hauteurs[k]   = heightCoarse(x, z) * (1.0f - t) + hauteurs[k] * t;
        }
    }
}

void Terrain::suivreDetail(float x, float z, float dt) {
    if (m_detail) {
        m_detail->suivre(x, z, dt);
    }
    if (m_detailFin) {
        m_detailFin->suivre(x, z, dt);
    }
    if (m_relief) {
        m_relief->suivre(x, z);
    }
}

Terrain::~Terrain() {
    if (m_carteRelief != 0) {
        glDeleteTextures(1, &m_carteRelief);
    }
}

void Terrain::buildPadPlatforms() {
    /* Hauteur du plateau = point le plus HAUT du relief sous l'emprise du pad
       (centre + deux anneaux d'échantillons), lu AVANT d'enregistrer la
       plate-forme (les appels à heightAt ne sont donc pas influencés par elle).
       Caler sur le seul centre laissait, sur un terrain en pente, le relief
       amont crever le disque ; calé sur le maximum, le disque coiffe tout le
       relief de son emprise et la jupe habille le côté aval. Les pads hors
       emprise sont ignorés, comme au dessin. */
    if (!m_hasGeo) {
        return;
    }
    constexpr float TWO_PI = 6.2831853f;
    constexpr int SAMPLES = 16; /* par anneau : assez serré pour des mailles de ~17 m */
    const float halfW = 0.5f * m_widthM;
    const float halfH = 0.5f * m_heightM;
    m_padPlatforms.reserve(m_helipads.size());
    for (const Landmark& pad : m_helipads) {
        float x = 0.0f, z = 0.0f;
        worldAt(pad.lon, pad.lat, x, z);
        /* worldAt() renvoie des coordonnées MONDE ; l'emprise [-halfW,halfW] est
           locale (centrée sur m_originX/m_originZ) : sans la soustraction, un pad
           hors du centre de la carte (recadrée) est cru hors emprise et n'obtient
           jamais sa plate-forme anti-enfoncement. */
        if (std::fabs(x - m_originX) > halfW || std::fabs(z - m_originZ) > halfH) {
            continue;
        }
        float top = heightAt(x, z);
        for (int ring = 1; ring <= 2; ++ring) {
            const float r = PAD_PLATFORM_RADIUS_M * static_cast<float>(ring) / 2.0f;
            for (int i = 0; i < SAMPLES; ++i) {
                const float a = TWO_PI * static_cast<float>(i) / static_cast<float>(SAMPLES);
                top = std::fmax(top, heightAt(x + r * std::cos(a), z + r * std::sin(a)));
            }
        }
        m_padPlatforms.push_back({x, z, top});
    }
}

} /* namespace artouste::render */
