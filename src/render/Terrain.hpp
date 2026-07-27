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

#include "render/Bc7.hpp"
#include "render/Mesh.hpp"
#include "render/Texture.hpp"
#include "render/hapi/Hapi.hpp"
#include "render/tuiles/Fenetre.hpp"

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace artouste::render {

/* Lieu remarquable du terrain (nom + position WGS84) : étiqueté sur la scène et
   pointé sur la minimap. Chaque terrain a ses propres lieux, lus de landmarks.txt
   dans son dossier. */
struct Landmark {
    std::string name;
    float lon = 0.0f;
    float lat = 0.0f;
};

class Terrain {
public:
    /* Charge le terrain depuis un dossier contenant terrain.txt, heightmap.png
       et ortho.jpg. En cas d'absence, construit un damier plat de repli.

       Le rappel de progression n'est utilisé qu'au tout premier chargement
       d'une carte, quand l'orthophoto doit être compressée avant sa mise en
       cache (voir TextureCache.hpp) : c'est la seule étape assez longue pour
       mériter d'être montrée. Il peut renvoyer faux pour annuler.

       fenetreDetailPx fixe le côté de la fenêtre de tuiles fines, si la carte en
       livre (voir tuiles/Fenetre.hpp) ; 0 y renonce.

       sommetsMax plafonne le nombre de sommets DESSINÉS : au-delà, un point de
       grille sur deux, sur trois... est retenu pour le maillage. Les altitudes,
       elles, restent lues en entier par heightAt, qui porte le poser, les
       plates-formes d'hélisurface et la collision : une carte d'altitude fine
       améliore donc le vol même quand elle est allégée au dessin. 0 dessine
       tous les points. */
    explicit Terrain(const std::filesystem::path& dir,
                     bc7::Progression             progression     = {},
                     int                          fenetreDetailPx = tuiles::COTE_FENETRE_PX,
                     int                          sommetsMax      = 0);

    void draw() const { m_mesh.draw(); }

    /* Vrai si l'orthophoto est chargée : le rendu doit alors utiliser le shader
       de terrain texturé ; sinon, le shader à couleurs de sommets suffit. */
    [[nodiscard]] bool textured() const noexcept { return m_textured; }

    /* Active l'orthophoto sur l'unité de texture donnée. */
    void bindTexture(unsigned int unit = 0) const { m_ortho.bind(unit); }

    /* Altitude du sol (m) sous le point (x, z) du repère monde, par
       interpolation bilinéaire. Renvoie 0 (niveau de la mer) hors de l'emprise.
       Définie dans TerrainQuery.cpp. */
    [[nodiscard]] float heightAt(float x, float z) const noexcept;

    [[nodiscard]] float halfWidth() const noexcept { return 0.5f * m_widthM; }
    [[nodiscard]] float halfHeight() const noexcept { return 0.5f * m_heightM; }
    /* Centre de l'emprise en coordonnées monde (0 sauf carte recadrée). */
    [[nodiscard]] float originX() const noexcept { return m_originX; }
    [[nodiscard]] float originZ() const noexcept { return m_originZ; }
    [[nodiscard]] float maxElevation() const noexcept { return m_elevMax; }

    /* Finesse réelle de l'orthophoto, en mètres au sol par pixel de texture.
       Mesurée sur la texture chargée plutôt que lue dans terrain.txt, pour
       rester juste même si le fichier de calage ment. Renvoie 0 si aucune
       orthophoto n'est chargée. terrain.frag s'en sert pour ne plus ajouter de
       grain de synthèse là où la photo a déjà le sien. */
    [[nodiscard]] float orthoMetersPerPixel() const noexcept {
        return (m_ortho.height() > 0) ? m_heightM / static_cast<float>(m_ortho.height()) : 0.0f;
    }

    /* Faut-il dessiner le plan de mer ? Faux pour un terrain de montagne (sans mer),
       où un plan bleu sous le relief serait incongru. Vrai par défaut (bord de mer). */
    [[nodiscard]] bool drawsSea() const noexcept { return m_drawSea; }

    /* Le calage fournit-il un point de départ propre au terrain (sinon l'appelant
       garde le sien) ? Coordonnées monde en mètres (X est, Z sud). */
    [[nodiscard]] bool hasStart() const noexcept { return m_hasStart; }
    [[nodiscard]] float startX() const noexcept { return m_startX; }
    [[nodiscard]] float startZ() const noexcept { return m_startZ; }
    [[nodiscard]] float startHeadingDeg() const noexcept { return m_startHeadingDeg; }

    /* Le calage fournit-il les bornes géographiques (longitude / latitude) ? */
    [[nodiscard]] bool hasGeo() const noexcept { return m_hasGeo; }

    /* Longitude et latitude (degrés WGS84) d'un point du monde (x = est, z = sud).
       L'emprise est centrée sur l'origine : colonne ouest = -halfWidth, rangée nord
       = -halfHeight. */
    void lonLatAt(float x, float z, float& lon, float& lat) const noexcept {
        const float colFrac = (x - m_originX) / m_widthM + 0.5f;
        const float rowFrac = (z - m_originZ) / m_heightM + 0.5f;
        lon = m_lonMin + colFrac * (m_lonMax - m_lonMin);
        lat = m_latMax - rowFrac * (m_latMax - m_latMin);
    }

    /* Conversion inverse : longitude / latitude -> position au sol (x est, z sud). */
    void worldAt(float lon, float lat, float& x, float& z) const noexcept {
        const float colFrac = (lon - m_lonMin) / (m_lonMax - m_lonMin);
        const float rowFrac = (m_latMax - lat) / (m_latMax - m_latMin);
        x = (colFrac - 0.5f) * m_widthM + m_originX;
        z = (rowFrac - 0.5f) * m_heightM + m_originZ;
    }

    /* Identifiant OpenGL de l'orthophoto (pour l'afficher dans la minimap). */
    [[nodiscard]] unsigned int orthoTexId() const noexcept { return m_ortho.id(); }

    /* Fenêtres de détail de ce terrain, ou nullptr (voir tuiles/Fenetre.hpp).
       La première couvre l'emprise de la carte ; la seconde, plus fine, n'existe
       que là où la carte fournit un niveau serré (abords des aires de poser,
       fond de vallée). Le rendu s'en sert pour ses uniformes ; tout le reste du
       moteur peut les ignorer. */
    [[nodiscard]] const tuiles::Fenetre* detail() const noexcept { return m_detail.get(); }
    [[nodiscard]] const tuiles::Fenetre* detailFin() const noexcept { return m_detailFin.get(); }

    /* Recentre la fenêtre de détail sur un point du monde (en pratique la
       caméra) et fait avancer ses chargements. Sans effet si la carte n'a pas
       de tuiles. À appeler une fois par image. */
    void suivreDetail(float x, float z, float dt);

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
       le guide DGAC/STAC), jamais ailleurs sur le terrain. Définie dans
       TerrainQuery.cpp. */
    [[nodiscard]] const HapiUnit* hapiUnitNear(float lon, float lat, float maxDistM) const noexcept;

private:
    void buildFlatFallback();
    /* Ouvre le jeu de tuiles de détail de la carte, s'il y en a un, et prépare
       sa fenêtre au côté demandé. Appelée une fois, après l'orthophoto
       d'ensemble. */
    void ouvrirDetail(const std::filesystem::path& dir, int fenetrePx);
    /* Charge un fichier de lieux "lon lat nom" (un par ligne) dans out. Fichier
       absent : out reste vide. label sert à la trace affichée. */
    void
    loadPlaces(const std::filesystem::path& path, std::vector<Landmark>& out, const char* label);
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

    Mesh m_mesh;
    Texture m_ortho;
    /* Tuiles fines de la carte, si elle en livre : le niveau large d'abord, le
       niveau serré ensuite. Non déplaçables (ressources OpenGL et fil de
       lecture), d'où les pointeurs. */
    std::unique_ptr<tuiles::Fenetre> m_detail;
    std::unique_ptr<tuiles::Fenetre> m_detailFin;
    bool m_textured = false;
    /* Rappel de progression de la préparation de l'orthophoto, transmis au
       cache. Vide en dehors du premier chargement d'une carte. */
    bc7::Progression m_progression;

    /* Carte d'altitude conservée côté CPU pour interroger la hauteur du sol.
       Rangée 0 = nord (latitude max), colonne 0 = ouest (longitude min). */
    std::vector<float> m_heights;
    int m_cols = 0;
    int m_rows = 0;
    float m_widthM = 0.0f;  /* dimension est-ouest au sol (m) */
    float m_heightM = 0.0f; /* dimension nord-sud au sol (m) */
    /* Décalage de l'origine du terrain en coordonnées monde (m) : par défaut 0,
       l'emprise est centrée sur l'origine du monde. Non nul pour une carte
       RECADRÉE (sous-région d'un terrain plus grand) : la grille reste centrée
       sur (m_originX, m_originZ) au lieu de (0,0), si bien que tous les fichiers
       en coordonnées monde (spawns, bâtiments, hélipads...) restent valides sans
       décalage. Voir terrain.txt "origin_x"/"origin_z". */
    float m_originX = 0.0f;
    float m_originZ = 0.0f;
    float m_elevMin = 0.0f;
    float m_elevMax = 0.0f;
    bool m_drawSea = true;           /* dessiner le plan de mer (bord de mer) */
    bool m_hasStart = false;         /* le calage fournit un point de départ */
    float m_startX = 0.0f;           /* point de départ : est (m) */
    float m_startZ = 0.0f;           /* point de départ : sud (m) */
    float m_startHeadingDeg = 90.0f; /* cap initial (boussole) ; 90 = est (identité) */
    bool m_hasGeo = false;           /* le calage fournit les bornes lon/lat */
    float m_lonMin = 0.0f;
    float m_lonMax = 0.0f;
    float m_latMin = 0.0f;
    float m_latMax = 0.0f;

    std::vector<Landmark> m_landmarks; /* lieux remarquables propres au terrain */
    std::vector<Landmark> m_helipads;  /* hélipads propres au terrain (hors départ) */
    std::vector<HapiUnit> m_hapiUnits; /* balises HAPI propres au terrain */

    /* Plate-forme porteuse d'un hélipad : centre monde et hauteur du plateau. */
    struct PadPlatform {
        float x = 0.0f;
        float z = 0.0f;
        float top = 0.0f;
    };
    std::vector<PadPlatform> m_padPlatforms;
};

} /* namespace artouste::render */
