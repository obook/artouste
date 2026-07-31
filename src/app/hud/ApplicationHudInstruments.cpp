/*
 * ApplicationHudInstruments.cpp
 * Construction des données d'instruments du HUD (altitude, vitesse, cap,
 * régimes, carburant...) lues dans l'état physique. L'aide à l'atterrissage
 * (réticule de centrage et score au posé) vit dans
 * ApplicationHudLanding.cpp ; le repérage (minimap et étiquettes des lieux
 * remarquables) dans ApplicationHudNav.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "app/CycleJourNuit.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"

#include <cmath>

namespace artouste::app {

void Application::fillHud(ui::HudData& hud,
                          const physics::RigidBody& body,
                          const vec3& forward,
                          const physics::Controls& controls,
                          float airspeed,
                          float turbineFraction,
                          float rotorFraction,
                          float t,
                          float frameDt) {
    hud.altitudeM = body.position.y;
    hud.airspeedKmh = airspeed * 3.6f; /* m/s -> km/h (unité d'époque de l'Alouette II FR) */
    /* Cap boussole pour le HUD (ruban de cap, texte HDG, flèche de la minimap) :
       0 = nord, 90 = est, sens horaire. Le repère monde a X vers l'est et Z vers
       le sud, donc le nord est -Z. (Le lacet 'yaw' calculé dans la boucle, mesuré
       depuis l'est, reste réservé à la caméra de poursuite.) */
    float headingDeg = glm::degrees(std::atan2(forward.x, -forward.z));
    if (headingDeg < 0.0f) {
        headingDeg += 360.0f;
    }
    hud.headingDeg = headingDeg;
    hud.varioMs = body.velocity.y;
    hud.collectivePct = controls.collective * 100.0f;
    hud.rotorPct = rotorFraction * 100.0f; /* régime rotor, en pourcentage */
    hud.rotorRpm = rotorFraction * 360.0f; /* régime rotor nominal : 360 tr/min */
    /* LED du cadran NR : armée quand le rotor atteint la bande nominale (340 tr/min,
       bas de la bande verte du cadran), désarmée dès que la turbine quitte son régime.
       Pendant le démarrage et l'extinction, le NR est légitimement bas : LED éteinte
       plutôt que rouge en permanence sur le pad. */
    if (m_flight.turbine().state() != physics::Turbine::State::Regime) {
        m_nrLedArmed = false;
    } else if (hud.rotorRpm >= 340.0f) {
        m_nrLedArmed = true;
    }
    hud.rotorLedArmed = m_nrLedArmed;
    /* LED du cadran NR clignotante pendant la montée en régime du rotor (état Embrayage :
       frein lâché, le rotor accélère jusqu'au régime de vol), plutôt qu'éteinte. */
    hud.rotorSpoolingUp = (m_flight.turbine().state() == physics::Turbine::State::Embrayage);
    hud.turbineRpm = turbineFraction * 33500.0f; /* régime turbine nominal : ~33 500 tr/min */
    /* LED du cadran TURBINE clignotante pendant la montée en régime (état Demarrage : la
       turbine seule accélère, rotor encore immobile), plutôt qu'éteinte comme à l'arrêt. */
    hud.turbineSpoolingUp = (m_flight.turbine().state() == physics::Turbine::State::Demarrage);
    hud.exhaustTempC = m_flight.turbine().exhaustTempC(); /* température tuyère (T4) */
    hud.fuelLiters = m_flight.fuelLiters();
    hud.turbine = m_flight.turbine().label();
    hud.assist = m_assist.active();
    hud.autoland = m_autoland.active();
    hud.vrsIntensity = m_flight.vrsIntensity(); /* alerte vortex (bandeau HUD) */

    /* Alerte taux de descente (façon GPWS) : descente rapide près du sol. Enveloppe
       progressive (physics::gpwsSinkLimitMs) : plus on est bas, plus le taux de
       descente toléré est faible. Au-dessus de GPWS_MAX_AGL, aucune alerte (descente
       rapide normale en altitude). Coupée pendant la croisière de la démo (points de
       passage, hors sujet ici) mais PAS pendant son retour au pad ni pendant
       l'atterrissage automatique : ces deux phases visent maintenant un taux de
       descente sous ce même seuil (collectifApprocheGpws, DemoPilotDetail.hpp), donc
       l'alerte ne devrait normalement plus s'y déclencher plutôt que d'être masquée. */
    {
        const float aglM = body.position.y - m_terrain->heightAt(body.position.x, body.position.z);
        const float sink = -hud.varioMs; /* positif en descente */
        const bool surveille = !m_demo.active() || m_demo.returning();
        bool alert = false;
        if (surveille && aglM > physics::GPWS_MIN_AGL && aglM < physics::GPWS_MAX_AGL) {
            alert = sink > physics::gpwsSinkLimitMs(aglM);
        }
        hud.sinkRateAlert = alert;
    }

    hud.radio = m_audio.radioPlaying();
    hud.radioMixPct = static_cast<int>(m_audio.radioMix() * 100.0f + 0.5f);
    if (m_terrain->hasGeo()) { /* longitude / latitude de l'appareil */
        float lon = 0.0f, lat = 0.0f;
        m_terrain->lonLatAt(body.position.x, body.position.z, lon, lat);
        hud.geoValid = true;
        hud.lonDeg = lon;
        hud.latDeg = lat;
    }
    /* Heure du simulateur (réelle ou accélérée) : voir Application::timeOfDaySeconds.
       Le deux-points clignote à 1 Hz sur le temps réel écoulé (allumé une demi-seconde
       sur deux), comme une horloge digitale. */
    hud.timeOfDaySec = timeOfDaySeconds(t);
    /* Facteur affiché : celui qui s'applique à l'heure courante, donc multiplié par
       lune_vitesse entre le coucher et le lever. Afficher m_sunTimeScale tel quel
       annonçait x72 en pleine nuit pendant que l'horloge tournait à x144, ce qui
       donnait à croire que la nuit n'était pas accélérée. */
    hud.timeScale = vitesseCourante(m_sunTimeScale, m_nightSpeedFactor, hud.timeOfDaySec);
    hud.colonOn = (std::fmod(t, 1.0f) < 0.5f);
    /* Clignotement des LED d'alarme jaune/rouge : cadence rapide (~2 Hz, allumée un peu
       plus de la moitié du temps) pour accrocher l'oeil, distincte du deux-points à 1 Hz. */
    hud.alarmBlinkOn = (std::fmod(t, 0.5f) < 0.3f);

    /* Cadence affichée (images/s) : moyenne mobile exponentielle du frameDt instantané,
       pour un chiffre stable. Le frameDt est borné à 0,1 s dans la boucle (plancher à
       10 Hz), donc la valeur sature vers 10 en cas de forte chute -- justement le seuil
       sous lequel le simulateur passe au ralenti. */
    if (frameDt > 0.0f) {
        const float instantFps = 1.0f / frameDt;
        m_fpsSmoothed = (m_fpsSmoothed <= 0.0f)
                            ? instantFps
                            : m_fpsSmoothed + (instantFps - m_fpsSmoothed) * 0.1f;
    }
    hud.fps = m_fpsSmoothed;

    /* Aide à l'atterrissage : réticule de centrage sur le pad le plus proche et
       score au posé (voir ApplicationHudLanding.cpp). */
    updateLandingAid(hud, body, forward, frameDt);

    /* Mode zombie : vie, munitions, vague, chrono, plafond d'altitude et fin
       de partie (voir CombatMode). hud.combat.active reste faux (valeur par
       défaut) sur les cartes/modes sans combat : HudCombat.cpp ne dessine
       alors rien. */
    hud.combat.active = m_combat.active();
    if (hud.combat.active) {
        hud.combat.healthPct = m_combat.healthPct();
        hud.combat.ammoCurrent = m_combat.ammo();
        hud.combat.ammoMax = m_combat.ammoMax();
        hud.combat.wave = m_combat.wave();
        hud.combat.elapsedS = m_combat.elapsedS();
        hud.combat.belowCeiling = m_combat.belowCeiling();
        hud.combat.gameOver = m_combat.gameOver();
        hud.combat.score = m_combat.score();
        hud.combat.kills = m_combat.kills();
        using KillAnnouncement = CombatMode::KillAnnouncement;
        switch (m_combat.killAnnouncement()) {
            case KillAnnouncement::Double:
                hud.combat.killAnnounceKind = 1;
                break;
            case KillAnnouncement::Triple:
                hud.combat.killAnnounceKind = 2;
                break;
            case KillAnnouncement::Carnage:
                hud.combat.killAnnounceKind = 3;
                break;
            case KillAnnouncement::Brood:
                hud.combat.killAnnounceKind = 4;
                break;
            case KillAnnouncement::None:
                hud.combat.killAnnounceKind = 0;
                break;
        }
        hud.combat.broodActive = m_combat.broodActive();
        hud.combat.broodHealthPct = m_combat.broodHealthPct();

        /* Mire : projette un point loin devant l'appareil dans l'axe de tir
           (repère corps, canon fixe -- voir CombatMode::update, même axe que
           fireDir) selon la caméra active, même technique que les étiquettes
           de lieux (ApplicationHudNav.cpp::addLabel). */
        constexpr float RETICLE_DISTANCE_M = 150.0f;
        const vec3 aimPoint = body.position + forward * RETICLE_DISTANCE_M;
        const mat4 viewProj = m_camera.proj() * m_camera.view();
        const vec4 clip = viewProj * vec4{aimPoint, 1.0f};
        hud.combat.reticleOnScreen = false;
        if (clip.w > 0.1f) {
            const vec3 ndc = vec3(clip) / clip.w;
            if (ndc.z < 1.0f && std::fabs(ndc.x) < 1.02f && std::fabs(ndc.y) < 1.02f) {
                hud.combat.reticleFx = ndc.x * 0.5f + 0.5f;
                hud.combat.reticleFy = 1.0f - (ndc.y * 0.5f + 0.5f);
                hud.combat.reticleOnScreen = true;
            }
        }
    }
}

} /* namespace artouste::app */
