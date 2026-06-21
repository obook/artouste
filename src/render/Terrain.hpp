/*
 * Terrain.hpp
 * Terrain réel de la vallée d'Ossau (lac d'Artouste, pic du Midi d'Ossau). Le
 * relief vient d'une carte d'altitude (heightmap) issue du MNT de l'IGN, et
 * l'orthophoto réelle est drapée dessus comme une texture. Si les données sont
 * absentes, on retombe sur un simple damier plat afin que le simulateur reste
 * utilisable.
 *
 * Repère monde du projet : X vers l'est, Z vers le sud, Y vers le haut. Le
 * terrain est centré sur l'origine.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Mesh.hpp"
#include "render/Texture.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace artouste::render {

/* Lieu remarquable du terrain (nom + position WGS84) : étiqueté sur la scène et
   pointé sur la minimap. Chaque terrain a ses propres lieux, lus de landmarks.txt
   dans son dossier. */
struct Landmark {
    std::string name;
    float       lon = 0.0f;
    float       lat = 0.0f;
};

class Terrain {
public:
    /* Charge le terrain depuis un dossier contenant terrain.txt, heightmap.png
       et ortho.jpg. En cas d'absence, construit un damier plat de repli. */
    explicit Terrain(const std::filesystem::path& dir);

    void draw() const { m_mesh.draw(); }

    /* Vrai si l'orthophoto est chargée : le rendu doit alors utiliser le shader
       de terrain texturé ; sinon, le shader à couleurs de sommets suffit. */
    [[nodiscard]] bool textured() const noexcept { return m_textured; }

    /* Active l'orthophoto sur l'unité de texture donnée. */
    void bindTexture(unsigned int unit = 0) const { m_ortho.bind(unit); }

    /* Altitude du sol (m) sous le point (x, z) du repère monde, par
       interpolation bilinéaire. Renvoie 0 (niveau de la mer) hors de l'emprise. */
    [[nodiscard]] float heightAt(float x, float z) const noexcept;

    [[nodiscard]] float halfWidth() const noexcept { return 0.5f * m_widthM; }
    [[nodiscard]] float halfHeight() const noexcept { return 0.5f * m_heightM; }
    [[nodiscard]] float maxElevation() const noexcept { return m_elevMax; }

    /* Faut-il dessiner le plan de mer ? Faux pour un terrain de montagne (sans mer),
       où un plan bleu sous le relief serait incongru. Vrai par défaut (bord de mer). */
    [[nodiscard]] bool drawsSea() const noexcept { return m_drawSea; }

    /* Le calage fournit-il un point de départ propre au terrain (sinon l'appelant
       garde le sien) ? Coordonnées monde en mètres (X est, Z sud). */
    [[nodiscard]] bool  hasStart() const noexcept { return m_hasStart; }
    [[nodiscard]] float startX() const noexcept { return m_startX; }
    [[nodiscard]] float startZ() const noexcept { return m_startZ; }

    /* Le calage fournit-il les bornes géographiques (longitude / latitude) ? */
    [[nodiscard]] bool hasGeo() const noexcept { return m_hasGeo; }

    /* Longitude et latitude (degrés WGS84) d'un point du monde (x = est, z = sud).
       L'emprise est centrée sur l'origine : colonne ouest = -halfWidth, rangée nord
       = -halfHeight. */
    void lonLatAt(float x, float z, float& lon, float& lat) const noexcept {
        const float colFrac = x / m_widthM + 0.5f;
        const float rowFrac = z / m_heightM + 0.5f;
        lon = m_lonMin + colFrac * (m_lonMax - m_lonMin);
        lat = m_latMax - rowFrac * (m_latMax - m_latMin);
    }

    /* Conversion inverse : longitude / latitude -> position au sol (x est, z sud). */
    void worldAt(float lon, float lat, float& x, float& z) const noexcept {
        const float colFrac = (lon - m_lonMin) / (m_lonMax - m_lonMin);
        const float rowFrac = (m_latMax - lat) / (m_latMax - m_latMin);
        x = (colFrac - 0.5f) * m_widthM;
        z = (rowFrac - 0.5f) * m_heightM;
    }

    /* Identifiant OpenGL de l'orthophoto (pour l'afficher dans la minimap). */
    [[nodiscard]] unsigned int orthoTexId() const noexcept { return m_ortho.id(); }

    /* Lieux remarquables propres à ce terrain (vide si le terrain n'en fournit pas). */
    [[nodiscard]] const std::vector<Landmark>& landmarks() const noexcept { return m_landmarks; }

    /* Hélipads propres à ce terrain (hôpitaux, ports... où poser l'appareil).
       Vide si le terrain n'en fournit pas ; ne compte pas l'hélipad de départ. */
    [[nodiscard]] const std::vector<Landmark>& helipads() const noexcept { return m_helipads; }

private:
    void buildFlatFallback();
    /* Charge un fichier de lieux "lon lat nom" (un par ligne) dans out. Fichier
       absent : out reste vide. label sert à la trace affichée. */
    void loadPlaces(const std::filesystem::path& path, std::vector<Landmark>& out,
                    const char* label);
    /* Aplanit le relief sous le point de départ et chaque hélipad (plateforme
       plate, pour que sol, disque et appareil posé soient à la même hauteur). */
    void flattenPads();
    /* Met à plat les noeuds du relief dans un rayon (mètres) autour de la cellule
       (colf, rowf), à la hauteur interpolée en ce point. */
    void flattenAround(float colf, float rowf, float radiusM);

    Mesh    m_mesh;
    Texture m_ortho;
    bool    m_textured = false;

    /* Carte d'altitude conservée côté CPU pour interroger la hauteur du sol.
       Rangée 0 = nord (latitude max), colonne 0 = ouest (longitude min). */
    std::vector<float> m_heights;
    int                m_cols    = 0;
    int                m_rows    = 0;
    float              m_widthM  = 0.0f;  /* dimension est-ouest au sol (m) */
    float              m_heightM = 0.0f;  /* dimension nord-sud au sol (m) */
    float              m_elevMin = 0.0f;
    float              m_elevMax = 0.0f;
    bool               m_drawSea = true;   /* dessiner le plan de mer (bord de mer) */
    bool               m_hasStart = false; /* le calage fournit un point de départ */
    float              m_startX = 0.0f;    /* point de départ : est (m) */
    float              m_startZ = 0.0f;    /* point de départ : sud (m) */
    bool               m_hasGeo = false;   /* le calage fournit les bornes lon/lat */
    float              m_lonMin = 0.0f;
    float              m_lonMax = 0.0f;
    float              m_latMin = 0.0f;
    float              m_latMax = 0.0f;

    std::vector<Landmark> m_landmarks;  /* lieux remarquables propres au terrain */
    std::vector<Landmark> m_helipads;   /* hélipads propres au terrain (hors départ) */
};

}  /* namespace artouste::render */
