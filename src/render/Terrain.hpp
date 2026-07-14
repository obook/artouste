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

#include <cmath>
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

/* Balise HAPI (Helicopter Approach Path Indicator) : feu au sol à 4 secteurs
   verticaux (vert clignotant/fixe, rouge fixe/clignotant) qui indique au pilote
   sa position par rapport à la pente d'approche visée. Chaque terrain a ses
   propres balises, lues de hapi.txt dans son dossier (fichier absent : aucune). */
struct HapiUnit {
    std::string name;
    float       lon          = 0.0f;
    float       lat          = 0.0f;
    float       azimuthDeg   = 0.0f;  /* cap d'approche suivi par le pilote (0 = nord) */
    float       slopePercent = 6.0f;  /* pente d'approche visée (%), 6 % = valeur usuelle */
};

/* Rayon porteur d'une plate-forme d'hélisurface (m) : un peu plus grand que le
   disque dessiné (7 m), pour que les patins restent portés posé un peu décalé.
   Dans ce rayon, heightAt ne descend jamais sous le plateau du pad. */
inline constexpr float PAD_PLATFORM_RADIUS_M = 8.0f;

/* Hauteur du mât d'une balise HAPI au-dessus du sol (m) : le guide DGAC/STAC
   montre un petit trépied. Partagée entre le rendu 3D (ApplicationGround.cpp)
   et le repérage HUD (ApplicationHud.cpp), qui doivent viser le même point. */
inline constexpr float HAPI_MAST_M = 1.0f;

/* Période et fraction "allumée" du clignotement HAPI (s) : partagées pour que
   la lueur au sol et le point d'étiquette clignotent en phase. */
inline constexpr float HAPI_BLINK_PERIOD_S = 1.0f;
inline constexpr float HAPI_BLINK_ON_FRAC  = 0.5f;

/* Vrai pendant la phase "allumée" du clignotement HAPI à l'instant donné. */
[[nodiscard]] inline bool hapiBlinkOn(float timeSeconds) noexcept {
    return std::fmod(timeSeconds, HAPI_BLINK_PERIOD_S) < HAPI_BLINK_PERIOD_S * HAPI_BLINK_ON_FRAC;
}

/* Secteur HAPI vu depuis la balise en direction d'un point (distance horizontale
   et écart vertical par rapport à elle), comparé à la pente d'approche visée.
   Seuils repris du guide DGAC/STAC (§ 4.4) : 22'30" de part et d'autre de la
   pente pour "sur la pente", 15' de plus pour "légèrement trop bas". À l'aplomb
   de la balise (distance quasi nulle), l'angle n'est pas défini : on retombe sur
   "sur la pente" plutôt que d'afficher un secteur erratique. */
enum class HapiSector { TooHigh, OnSlope, SlightlyLow, TooLow };

[[nodiscard]] HapiSector hapiSector(const HapiUnit& hapi, float horizDistM,
                                    float vertDeltaM) noexcept;

/* Vert (sur/au-dessus de la pente) ou rouge (trop bas), et faux si ce secteur
   doit actuellement clignoter à l'arrêt (secteurs "trop haut"/"trop bas" en
   phase éteinte). Combine hapiSector() et hapiBlinkOn() pour les deux
   consommateurs (lueur 3D, points d'étiquette HUD). */
struct HapiGlow {
    bool green = true;
    bool off   = false;
};

[[nodiscard]] HapiGlow hapiGlow(HapiSector sector, float timeSeconds) noexcept;

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
    [[nodiscard]] float startHeadingDeg() const noexcept { return m_startHeadingDeg; }

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

    /* Balises HAPI propres à ce terrain. Vide si le terrain n'en fournit pas. */
    [[nodiscard]] const std::vector<HapiUnit>& hapiUnits() const noexcept { return m_hapiUnits; }

    /* Balise HAPI la plus proche de (lon, lat), si elle est à moins de maxDistM ;
       nullptr sinon. Sert à faire correspondre un hélipad à sa balise : une HAPI
       se trouve toujours à proximité immédiate de son pad (quelques mètres, voir
       le guide DGAC/STAC), jamais ailleurs sur le terrain. */
    [[nodiscard]] const HapiUnit* hapiUnitNear(float lon, float lat, float maxDistM) const noexcept;

private:
    void buildFlatFallback();
    /* Charge un fichier de lieux "lon lat nom" (un par ligne) dans out. Fichier
       absent : out reste vide. label sert à la trace affichée. */
    void loadPlaces(const std::filesystem::path& path, std::vector<Landmark>& out,
                    const char* label);
    /* Charge un fichier de balises HAPI "lon lat azimut_deg pente_pct nom" (un par
       ligne) dans out. Fichier absent : out reste vide. */
    void loadHapiUnits(const std::filesystem::path& path, std::vector<HapiUnit>& out);
    /* Aplanit le relief sous le point de départ (plateforme plate, pour que sol,
       disque et appareil posé soient à la même hauteur au spawn). */
    void flattenPads();
    /* Recense les plates-formes des hélipads du terrain : centre monde et hauteur
       du plateau porteur (relief au centre du pad), utilisées par heightAt. */
    void buildPadPlatforms();
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
    float              m_startHeadingDeg = 90.0f;  /* cap initial (boussole) ; 90 = est (identité) */
    bool               m_hasGeo = false;   /* le calage fournit les bornes lon/lat */
    float              m_lonMin = 0.0f;
    float              m_lonMax = 0.0f;
    float              m_latMin = 0.0f;
    float              m_latMax = 0.0f;

    std::vector<Landmark>  m_landmarks;  /* lieux remarquables propres au terrain */
    std::vector<Landmark>  m_helipads;   /* hélipads propres au terrain (hors départ) */
    std::vector<HapiUnit>  m_hapiUnits;  /* balises HAPI propres au terrain */

    /* Plate-forme porteuse d'un hélipad : centre monde et hauteur du plateau. */
    struct PadPlatform {
        float x   = 0.0f;
        float z   = 0.0f;
        float top = 0.0f;
    };
    std::vector<PadPlatform> m_padPlatforms;
};

}  /* namespace artouste::render */
