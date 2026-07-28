/*
 * TerrainSetup.cpp
 * Opérations annexes du terrain, appelées à la construction : chargement des lieux
 * remarquables et des hélipads (fichiers texte), aplanissement du relief sous les
 * aires de poser, et sol plat de secours en l'absence de données réelles. Le calage,
 * le maillage et heightAt() sont dans Terrain.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"

#include "render/Primitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace artouste::render {

void Terrain::loadPlaces(const std::filesystem::path& path, std::vector<Landmark>& out,
                         const char* label) {
    /* Format : un lieu par ligne "lon lat nom", le nom étant le reste de la ligne
       (il peut contenir des espaces). Ligne vide ou commençant par # ignorée. */
    std::ifstream file(path);
    if (!file) {
        return;  /* fichier absent pour ce terrain : tableau vide */
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        Landmark           lm;
        if (!(iss >> lm.lon >> lm.lat)) {
            continue;  /* ligne vide, commentaire ou mal formée */
        }
        std::getline(iss, lm.name);
        const std::size_t first = lm.name.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;  /* coordonnées sans nom : on ignore */
        }
        const std::size_t last = lm.name.find_last_not_of(" \t\r\n");
        lm.name                = lm.name.substr(first, last - first + 1);
        out.push_back(std::move(lm));
    }
    std::printf("[Terrain] %zu %s chargé(s).\n", out.size(), label);
}

void Terrain::loadHapiUnits(const std::filesystem::path& path, std::vector<HapiUnit>& out) {
    /* Format : une balise par ligne "lon lat azimut_deg pente_pct nom", le nom
       étant le reste de la ligne (il peut contenir des espaces). Ligne vide ou
       mal formée ignorée. */
    std::ifstream file(path);
    if (!file) {
        return;  /* fichier absent pour ce terrain : tableau vide */
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        HapiUnit           hapi;
        if (!(iss >> hapi.lon >> hapi.lat >> hapi.azimuthDeg >> hapi.slopePercent)) {
            continue;  /* ligne vide, commentaire ou mal formée */
        }
        std::getline(iss, hapi.name);
        const std::size_t first = hapi.name.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;  /* coordonnées sans nom : on ignore */
        }
        const std::size_t last = hapi.name.find_last_not_of(" \t\r\n");
        hapi.name               = hapi.name.substr(first, last - first + 1);
        out.push_back(std::move(hapi));
    }
    std::printf("[Terrain] %zu balise(s) HAPI chargée(s).\n", out.size());
}

void Terrain::loadMonuments(const std::filesystem::path& path, std::vector<Monument>& out) {
    /* Format : un monument par ligne
       "lon lat altitude cap echelle_h echelle_v rayon_m fichier nom", le nom
       étant le reste de la ligne (il peut contenir des espaces et des accents).
       Le champ altitude accepte un nombre (mètres) ou le mot-clé "sol", qui
       repose le modèle sur le relief. Ligne vide, commentaire ou mal formée
       ignorée. */
    std::ifstream file(path);
    if (!file) {
        return;  /* fichier absent pour ce terrain : tableau vide */
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        Monument           mon;
        std::string        altitude;
        if (!(iss >> mon.lon >> mon.lat >> altitude >> mon.headingDeg >> mon.scaleH >>
              mon.scaleV >> mon.clearRadiusM >> mon.file)) {
            continue;  /* ligne vide, commentaire ou mal formée */
        }
        if (altitude == "sol") {
            mon.onGround = true;
        } else {
            /* Altitude explicite : une valeur illisible disqualifie la ligne
               plutôt que de poser le monument à zéro sans prévenir. */
            try {
                mon.altitudeM = std::stof(altitude);
            } catch (const std::exception&) {
                std::fprintf(stderr,
                             "[Terrain] monuments.txt : altitude \"%s\" illisible, ligne ignorée.\n",
                             altitude.c_str());
                continue;
            }
        }
        /* Le chemin du modèle désigne un fichier RANGÉ SOUS
           assets/models/monuments/ : on refuse la remontée d'arborescence et le
           chemin absolu. Les cartes circulent en zip (voir la cible "cartes" du
           CMakeLists), donc un monuments.txt peut venir d'ailleurs que du dépôt :
           il n'a pas à désigner un fichier hors du dossier des modèles. */
        if (mon.file.find("..") != std::string::npos || mon.file.front() == '/' ||
            mon.file.find(':') != std::string::npos) {
            std::fprintf(stderr,
                         "[Terrain] monuments.txt : chemin \"%s\" hors du dossier des modèles, "
                         "ligne ignorée.\n",
                         mon.file.c_str());
            continue;
        }

        std::getline(iss, mon.name);
        const std::size_t first = mon.name.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;  /* monument sans nom : on ignore */
        }
        const std::size_t last = mon.name.find_last_not_of(" \t\r\n");
        mon.name                = mon.name.substr(first, last - first + 1);
        out.push_back(std::move(mon));
    }
    std::printf("[Terrain] %zu monument(s) 3D déclaré(s).\n", out.size());
}

void Terrain::flattenPads() {
    if (m_heights.empty() || m_cols < 2 || m_rows < 2) {
        return;
    }
    /* Rayon aplani : le disque (7 m) plus la longueur de l'appareil, pour qu'il
       repose à plat même posé un peu décalé sur le pad. Seul le point de départ
       déforme encore le relief : les hélipads du terrain sont des plates-formes
       portées par heightAt (voir buildPadPlatforms), car l'aplanissement couvrait
       au moins une maille entière (35 m et plus), très visible sur un sommet
       pointu (pic du Midi d'Ossau). */
    constexpr float PAD_RADIUS_M = 12.0f;
    if (m_hasStart) {
        /* Emprise centrée sur (m_originX, m_originZ) : coordonnées locales. */
        const float colf = ((m_startX - m_originX) / m_widthM + 0.5f) * static_cast<float>(m_cols - 1);
        const float rowf = ((m_startZ - m_originZ) / m_heightM + 0.5f) * static_cast<float>(m_rows - 1);
        flattenAround(colf, rowf, PAD_RADIUS_M);
    }
}

void Terrain::flattenAround(float colf, float rowf, float radiusM) {
    if (colf < 0.0f || colf > static_cast<float>(m_cols - 1) || rowf < 0.0f
        || rowf > static_cast<float>(m_rows - 1)) {
        return;  /* hélipad hors de l'emprise : rien à aplanir */
    }
    const auto H = [&](int c, int r) -> float& {
        return m_heights[static_cast<std::size_t>(r) * static_cast<std::size_t>(m_cols)
                         + static_cast<std::size_t>(c)];
    };

    /* Hauteur cible : altitude interpolée au centre du pad, lue avant modification. */
    const int   c0 = std::clamp(static_cast<int>(std::floor(colf)), 0, m_cols - 2);
    const int   r0 = std::clamp(static_cast<int>(std::floor(rowf)), 0, m_rows - 2);
    const float fc = colf - static_cast<float>(c0);
    const float fr = rowf - static_cast<float>(r0);
    const float target = (H(c0, r0) * (1.0f - fc) + H(c0 + 1, r0) * fc) * (1.0f - fr)
                         + (H(c0, r0 + 1) * (1.0f - fc) + H(c0 + 1, r0 + 1) * fc) * fr;

    /* Rayon converti en nombre de cellules sur chaque axe (la maille est plus
       large nord-sud qu'est-ouest), puis on met à plat tous les noeuds couverts. */
    const float dCol = radiusM / (m_widthM / static_cast<float>(m_cols - 1));
    const float dRow = radiusM / (m_heightM / static_cast<float>(m_rows - 1));
    const int   cmin = std::clamp(static_cast<int>(std::floor(colf - dCol)), 0, m_cols - 1);
    const int   cmax = std::clamp(static_cast<int>(std::ceil(colf + dCol)), 0, m_cols - 1);
    const int   rmin = std::clamp(static_cast<int>(std::floor(rowf - dRow)), 0, m_rows - 1);
    const int   rmax = std::clamp(static_cast<int>(std::ceil(rowf + dRow)), 0, m_rows - 1);
    for (int r = rmin; r <= rmax; ++r) {
        for (int c = cmin; c <= cmax; ++c) {
            H(c, r) = target;
        }
    }
}

void Terrain::buildFlatFallback() {
    /* Damier plat de secours, identique à l'ancien sol, en cas de données absentes. */
    m_cols = m_rows = 0;
    m_widthM = m_heightM = 800.0f;
    m_elevMin = m_elevMax = 0.0f;
    m_heights.clear();
    m_textured = false;

    const primitives::MeshData data =
        primitives::groundGrid(400.0f, 80, vec3{0.33f, 0.50f, 0.24f}, vec3{0.29f, 0.45f, 0.21f});
    m_mesh = Mesh(data.vertices, data.indices);
}

}  /* namespace artouste::render */
