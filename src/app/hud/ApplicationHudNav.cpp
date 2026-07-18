/*
 * ApplicationHudNav.cpp
 * Repérage du HUD : minimap (orthophoto, position de l'appareil) et étiquettes
 * des lieux remarquables et hélipads, projetées sur la scène. Les données
 * d'instruments (altitude, vitesse, aide à l'atterrissage...) vivent dans
 * ApplicationHudInstruments.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "render/Camera.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"
#include "util/Math.hpp"

#include <cmath>
#include <cstdio>

namespace artouste::app {

void Application::buildNavHud(ui::HudData& hud, const vec3& heliPos, float headingDeg,
                              float timeSeconds) {
    if (!m_terrain || !m_terrain->hasGeo()) {
        return;
    }
    const float halfW = m_terrain->halfWidth();
    const float halfH = m_terrain->halfHeight();
    /* Emprise centrée sur (origX, origZ) en monde (0 sauf carte recadrée). */
    const float origX = m_terrain->originX();
    const float origZ = m_terrain->originZ();

    /* Minimap : orthophoto + position de l'appareil (fractions 0-1 dans l'emprise). */
    hud.mapTexId      = m_terrain->orthoTexId();
    hud.mapHeliU      = (heliPos.x - origX) / (2.0f * halfW) + 0.5f;
    hud.mapHeliV      = (heliPos.z - origZ) / (2.0f * halfH) + 0.5f;
    /* headingDeg est déjà un cap boussole (0 = nord, 90 = est, sens horaire), ce
       qu'attend la flèche de la minimap (nord en haut). */
    hud.mapHeadingDeg = headingDeg;

    /* Étiquette 3D + point minimap d'un lieu (nom + position WGS84), s'il tombe dans
       l'emprise du terrain courant. Projetée légèrement au-dessus du sol. */
    const mat4 viewProj = m_camera.proj() * m_camera.view();
    const auto addLabel = [&](const render::Landmark& place, const char* displayName,
                              bool generic) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(place.lon, place.lat, x, z);
        if (std::fabs(x - origX) > halfW || std::fabs(z - origZ) > halfH) {
            return;  /* hors du terrain courant */
        }
        const float altSol = m_terrain->heightAt(x, z);  /* altitude sol du repère (m ASL) */

        /* Tout label (lieu ou hélipad) porte l'altitude sol du repère puis la distance
           3D qui le sépare de l'appareil. Altitude toujours en mètres. Distance en
           kilomètres au loin, mais repassée en mètres sous 1000 m : en approche "820 m"
           guide la pose là où "0.8 km" resterait trop grossier. Recalculé à chaque
           frame, donc la distance suit le vol en direct. */
        const float dx   = heliPos.x - x;
        const float dy   = heliPos.y - altSol;
        const float dz   = heliPos.z - z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        /* Deux lignes : le nom, puis "altitude  distance" en dessous. Le rendu
           (HudOverlay) centre chaque ligne séparément. */
        char buf[96];
        if (dist < 1000.0f) {
            std::snprintf(buf, sizeof buf, "%s\n%.0f m  %.0f m",
                          displayName, static_cast<double>(altSol), static_cast<double>(dist));
        } else {
            std::snprintf(buf, sizeof buf, "%s\n%.0f m  %.1f km",
                          displayName, static_cast<double>(altSol),
                          static_cast<double>(dist) / 1000.0);
        }

        ui::HudLabel label;
        label.name    = buf;
        label.generic = generic;
        label.mapU    = (x - origX) / (2.0f * halfW) + 0.5f;
        label.mapV    = (z - origZ) / (2.0f * halfH) + 0.5f;

        /* Pad équipé d'une balise HAPI toute proche (voir hapiUnitNear) : le point
           de l'étiquette adopte la couleur/clignotement de la balise plutôt que sa
           couleur par défaut (cyan générique ou doré pour un lieu nommé), pour lire
           la pente d'approche sans chercher la lueur au sol. Vaut aussi pour un lieu
           nommé qui coïncide avec le pad (l'aérodrome de Dax-Seyresse, par exemple) :
           sinon son point doré fixe resterait affiché à la place du vert/rouge HAPI,
           qui ne doit jamais laisser paraître d'ambre. Même calcul (secteur +
           clignotement) que la lueur 3D (Application::drawHapi), pour rester en phase
           avec elle. */
        if (const render::HapiUnit* hapi = m_terrain->hapiUnitNear(place.lon, place.lat, 100.0f)) {
            float hx = 0.0f, hz = 0.0f;
            m_terrain->worldAt(hapi->lon, hapi->lat, hx, hz);
            const float hy         = m_terrain->heightAt(hx, hz) + render::HAPI_MAST_M;
            const float hdx        = heliPos.x - hx;
            const float hdz        = heliPos.z - hz;
            const float horizDist  = std::sqrt(hdx * hdx + hdz * hdz);
            const render::HapiSector sector = render::hapiSector(*hapi, horizDist, heliPos.y - hy);
            const render::HapiGlow   glow   = render::hapiGlow(sector, timeSeconds);
            label.hasHapi   = true;
            label.hapiGreen = glow.green;
            label.hapiOff   = glow.off;
        }

        const float y    = altSol + 25.0f;
        const vec4  clip = viewProj * vec4{x, y, z, 1.0f};
        if (clip.w > 0.1f) {
            const vec3 ndc = vec3(clip) / clip.w;
            if (ndc.z < 1.0f && std::fabs(ndc.x) < 1.02f && std::fabs(ndc.y) < 1.02f) {
                label.fx       = ndc.x * 0.5f + 0.5f;
                label.fy       = 1.0f - (ndc.y * 0.5f + 0.5f);
                label.depth    = clip.w;  /* distance caméra : sert à donner la priorité au plus proche */
                label.onScreen = true;
            }
        }
        hud.labels.push_back(label);
    };

    /* Lieux remarquables (étiquetés par leur nom), puis hélipads (étiquetés
       "Hélisurface", le terme d'aire de poser ; leur ville est déjà donnée par le
       lieu remarquable voisin). Chaque étiquette porte en plus l'altitude du repère
       et sa distance à l'appareil (voir addLabel). L'étiquette d'hélipad est marquée
       générique : quand un pad coïncide avec un lieu nommé (sommet du pic du Midi
       d'Ossau, hélipads de bigorre...), c'est le nom du lieu qui reste lisible -- et
       il porte désormais lui aussi l'altitude et la distance. */
    /* Absentes en mode zombie : dans une arène confinée, les étiquettes de lieux
       (et leurs points sur la minimap) n'apporteraient rien et encombreraient
       l'écran pendant le combat. Les points rouges de la horde (ci-dessous)
       restent affichés : c'est le seul repérage utile ici. */
    if (!m_combat.active()) {
        for (const render::Landmark& lm : m_terrain->landmarks()) {
            addLabel(lm, lm.name.c_str(), false);
        }
        for (const render::Landmark& pad : m_terrain->helipads()) {
            addLabel(pad, "Hélisurface", true);
        }
    }

    /* Mode zombie : points de repérage sur la minimap (voir
       Hud::renderMinimap). Même formule u/v que le marqueur de l'appareil et
       les lieux remarquables ci-dessus, pas de vraie étiquette 3D -- l'usager
       a demandé un repérage sur la minimap, pas un nuage de noms qui
       encombrerait la scène avec une dizaine de zombies à l'écran. */
    if (m_combat.active()) {
        for (const vec3& zpos : m_combat.zombiePositions()) {
            if (std::fabs(zpos.x - origX) > halfW || std::fabs(zpos.z - origZ) > halfH) {
                continue;  /* hors de l'emprise du terrain courant */
            }
            ui::HudData::CombatHud::MapPoint pt;
            pt.u = (zpos.x - origX) / (2.0f * halfW) + 0.5f;
            pt.v = (zpos.z - origZ) / (2.0f * halfH) + 0.5f;
            hud.combat.zombieMapPoints.push_back(pt);
        }
    }
}

}  /* namespace artouste::app */
