/*
 * ApplicationHud.cpp
 * Construction des données du HUD : d'une part les instruments (altitude, vitesse,
 * cap, régimes, carburant...) lus dans l'état physique, d'autre part le repérage
 * (minimap et étiquettes des lieux remarquables projetées sur la scène).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "render/Camera.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app {

void Application::fillHud(ui::HudData& hud, const physics::RigidBody& body, const vec3& forward,
                          const physics::Controls& controls, float airspeed, float turbineFraction,
                          float rotorFraction) {
    hud.altitudeM  = body.position.y;
    hud.airspeedKt = airspeed * 1.94384f;
    /* Cap boussole pour le HUD (ruban de cap, texte HDG, flèche de la minimap) :
       0 = nord, 90 = est, sens horaire. Le repère monde a X vers l'est et Z vers
       le sud, donc le nord est -Z. (Le lacet 'yaw' calculé dans la boucle, mesuré
       depuis l'est, reste réservé à la caméra de poursuite.) */
    float headingDeg = glm::degrees(std::atan2(forward.x, -forward.z));
    if (headingDeg < 0.0f) {
        headingDeg += 360.0f;
    }
    hud.headingDeg    = headingDeg;
    hud.varioFpm      = body.velocity.y * 196.85f;
    hud.varioMs       = body.velocity.y;
    hud.collectivePct = controls.collective * 100.0f;
    hud.rotorPct      = rotorFraction * 100.0f;          /* régime rotor, en pourcentage */
    hud.rotorRpm      = rotorFraction * 360.0f;          /* régime rotor nominal : 360 tr/min */
    hud.turbineRpm    = turbineFraction * 33500.0f;      /* régime turbine nominal : ~33 500 tr/min */
    hud.exhaustTempC  = m_flight.turbine().exhaustTempC();  /* température tuyère (T4) */
    hud.fuelLiters    = m_flight.fuelLiters();
    hud.turbine       = m_flight.turbine().label();
    hud.assist        = m_assist.active();
    hud.radio         = m_audio.radioPlaying();
    hud.radioMixPct   = static_cast<int>(m_audio.radioMix() * 100.0f + 0.5f);
    if (m_terrain->hasGeo()) {  /* longitude / latitude de l'appareil */
        float lon = 0.0f, lat = 0.0f;
        m_terrain->lonLatAt(body.position.x, body.position.z, lon, lat);
        hud.geoValid = true;
        hud.lonDeg   = lon;
        hud.latDeg   = lat;
    }
}

void Application::buildNavHud(ui::HudData& hud, const vec3& heliPos, float headingDeg) {
    if (!m_terrain || !m_terrain->hasGeo()) {
        return;
    }
    const float halfW = m_terrain->halfWidth();
    const float halfH = m_terrain->halfHeight();

    /* Minimap : orthophoto + position de l'appareil (fractions 0-1 dans l'emprise). */
    hud.mapTexId      = m_terrain->orthoTexId();
    hud.mapHeliU      = heliPos.x / (2.0f * halfW) + 0.5f;
    hud.mapHeliV      = heliPos.z / (2.0f * halfH) + 0.5f;
    /* headingDeg est déjà un cap boussole (0 = nord, 90 = est, sens horaire), ce
       qu'attend la flèche de la minimap (nord en haut). */
    hud.mapHeadingDeg = headingDeg;

    /* Étiquette 3D + point minimap d'un lieu (nom + position WGS84), s'il tombe dans
       l'emprise du terrain courant. Projetée légèrement au-dessus du sol. */
    const mat4 viewProj = m_camera.proj() * m_camera.view();
    const auto addLabel = [&](const render::Landmark& place, const char* displayName) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(place.lon, place.lat, x, z);
        if (std::fabs(x) > halfW || std::fabs(z) > halfH) {
            return;  /* hors du terrain courant */
        }
        ui::HudLabel label;
        label.name = displayName;
        label.mapU = x / (2.0f * halfW) + 0.5f;
        label.mapV = z / (2.0f * halfH) + 0.5f;

        const float y    = m_terrain->heightAt(x, z) + 25.0f;
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

    /* Lieux remarquables (étiquetés par leur nom), puis hélipads (tous étiquetés
       "Hélisurface", le terme d'aire de poser ; leur ville est déjà donnée par le
       lieu remarquable voisin). */
    for (const render::Landmark& lm : m_terrain->landmarks()) {
        addLabel(lm, lm.name.c_str());
    }
    for (const render::Landmark& pad : m_terrain->helipads()) {
        addLabel(pad, "Hélisurface");
    }
}

}  /* namespace artouste::app */
