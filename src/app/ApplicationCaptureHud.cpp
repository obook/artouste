/*
 * ApplicationCaptureHud.cpp
 * Instruments affichés sur une capture : des valeurs de croisière plausibles,
 * réglables une à une par les variables ARTOUSTE_SHOT_*.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "app/ApplicationCaptureReglages.hpp"
#include "physics/constants.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"

#include <cstdlib>
#include <string>

namespace artouste::app {

ui::HudData Application::hudDeCapture(const vec3& shotPos) {
    ui::HudData hud;
    /* Vitesse de croisière affichée : ~170 km/h, mais plafonnée à 90 % de la VNE
       du moment (elle décroît avec l'altitude), pour que la LED IAS reste verte
       même sur une capture en altitude : sinon 170 km/h dépasse la VNE au-delà de
       ~2000 m (par ex. ~164 km/h à 2800 m) et le voyant passe au rouge. Réglable
       par ARTOUSTE_SHOT_IAS. */
    const float vneKmh = physics::vneAtAltitudeMs(shotPos.y) * 3.6f;
    const float cruise = 0.90f * vneKmh;
    hud.airspeedKmh   = (cruise < 170.0f) ? cruise : 170.0f;
    hud.airspeedKmh   = shotFloat("ARTOUSTE_SHOT_IAS", hud.airspeedKmh);
    hud.headingDeg    = 47.0f;
    hud.altitudeM     = shotPos.y;  /* vraie altitude du point de capture */
    hud.varioMs       = 1.2f;
    hud.collectivePct = 55.0f;
    hud.pasDeg        = 11.0f;    /* pas de sustentation à 55 % de collectif */
    hud.rotorPct      = 100.0f;
    hud.rotorRpm      = 360.0f;
    hud.rotorLedArmed = true;     /* rotor au régime : LED NR verte sur la capture */
    hud.turbineRpm    = 34000.0f;
    hud.exhaustTempC  = 445.0f;   /* tuyère en croisière normale */
    hud.fuelLiters    = 480.0f;
    hud.turbine       = "EN RÉGIME";
    if (shotFlag("ARTOUSTE_SHOT_TURBINE_OFF")) {
        /* Turbine à l'arrêt : tous les voyants doivent s'éteindre. */
        hud.rotorRpm      = 0.0f;
        hud.rotorLedArmed = false;
        hud.turbineRpm    = 0.0f;
        hud.turbine       = "ARRÊT";
    }
    hud.assist        = shotFlag("ARTOUSTE_SHOT_ASSIST");   /* repère "MODE ASSISTÉ" */
    hud.vrsIntensity  = shotFloat("ARTOUSTE_SHOT_VORTEX", hud.vrsIntensity);  /* bandeau vortex */
    hud.sinkRateAlert = shotFlag("ARTOUSTE_SHOT_SINKRATE"); /* bandeau taux de descente */
    if (m_terrain->hasGeo()) {  /* coordonnées du point de capture */
        float lon = 0.0f, lat = 0.0f;
        m_terrain->lonLatAt(shotPos.x, shotPos.z, lon, lat);
        hud.geoValid = true;
        hud.lonDeg   = lon;
        hud.latDeg   = lat;
    }
    buildNavHud(hud, shotPos, hud.headingDeg, 0.0f);  /* capture déterministe : phase "allumée" */

    return hud;
}

void Application::attendreTuilesDeDetail() {
    /* Tuiles de détail : en vol elles arrivent au fil des images, mais une
       capture n'en rend que trois. On laisse donc la fenêtre se remplir avant de
       photographier, sinon la carte serait immortalisée floue -- exactement ce
       qu'on cherche à vérifier. Plafond de temps pour ne jamais bloquer : une
       carte sans tuiles, ou un disque absent, ne doit pas empêcher la capture.
       Un dt généreux fait aussi terminer les fondus d'un coup. */
    if (m_terrain->detail() != nullptr) {
        const render::tuiles::Fenetre* large = m_terrain->detail();
        const render::tuiles::Fenetre* serree = m_terrain->detailFin();
        for (int i = 0; i < 400; ++i) {
            /* Le suivi d'abord : c'est lui qui recense les tuiles attendues, et
               le compte partirait de zéro si on le testait avant. */
            m_terrain->suivreDetail(m_camera.position().x, m_camera.position().z, 1.0f);
            if (large->stabilisee() && (serree == nullptr || serree->stabilisee())) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::printf("[capture] tuiles de détail : %d / %d en place",
                    large->residentes(),
                    large->attendues());
        if (serree != nullptr) {
            std::printf(", niveau serré %d / %d", serree->residentes(), serree->attendues());
        }
        std::printf(".\n");
    }
}

} /* namespace artouste::app */
