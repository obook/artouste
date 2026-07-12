/*
 * HudAlarms.hpp
 * États d'alarme des paramètres surveillés du HUD (vert = normal, jaune =
 * surveiller, rouge = limite franchie), partagés entre les deux affichages :
 * LED des cadrans du Super HUD (HudOverlay.cpp) et couleur des lignes de texte
 * du HUD 4 coins (Hud.cpp). Les seuils s'appuient sur les constantes physiques.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/constants.hpp"
#include "ui/Hud.hpp"
#include "ui/HudWidgets.hpp"

namespace artouste::ui::hud_widgets {

/* Régime rotor : autour de la bande nominale 340-380 tr/min du cadran. Alarme
 * inhibée (LED éteinte, texte vert) tant que le rotor n'a pas atteint son régime
 * depuis le dernier lancement de la turbine : pendant le démarrage et
 * l'extinction, un NR bas est normal. */
inline GaugeLed alarmeNr(const HudData& d) noexcept {
    if (!d.rotorLedArmed) {
        return GaugeLed::Off;
    }
    if (d.rotorRpm < 320.0f || d.rotorRpm > 400.0f) {
        return GaugeLed::Red;
    }
    if (d.rotorRpm < 340.0f || d.rotorRpm > 380.0f) {
        return GaugeLed::Yellow;
    }
    return GaugeLed::Green;
}

/* Régime turbine : verte dans la bande nominale du cadran (33 000-34 000 tr/min),
 * éteinte en dessous (démarrage, extinction, arrêt : un régime bas y est normal),
 * jaune puis rouge au-dessus (surrégime, théorique avec la régulation actuelle).
 * Sert surtout d'indicateur "turbine prête" pendant la séquence de démarrage. */
inline GaugeLed alarmeTurbine(const HudData& d) noexcept {
    if (d.turbineRpm > 34500.0f) {
        return GaugeLed::Red;
    }
    if (d.turbineRpm > 34000.0f) {
        return GaugeLed::Yellow;
    }
    if (d.turbineRpm >= 33000.0f) {
        return GaugeLed::Green;
    }
    return GaugeLed::Off;
}

/* Vitesse : sur la VNE réelle du moment, qui décroît avec l'altitude (le cadran
 * IAS, lui, garde une bande fixe). Jaune à partir de 90 % de la VNE. */
inline GaugeLed alarmeIas(const HudData& d) noexcept {
    const float vneKmh = physics::vneAtAltitudeMs(d.altitudeM) * 3.6f;
    if (d.airspeedKmh > vneKmh) {
        return GaugeLed::Red;
    }
    if (d.airspeedKmh > 0.9f * vneKmh) {
        return GaugeLed::Yellow;
    }
    return GaugeLed::Green;
}

/* Température tuyère (T4) : seuils du manuel (surveiller / limite continue). */
inline GaugeLed alarmeTmp(const HudData& d) noexcept {
    if (d.exhaustTempC > physics::EXHAUST_TEMP_MAXI_C) {
        return GaugeLed::Red;
    }
    if (d.exhaustTempC > physics::EXHAUST_TEMP_WARN_C) {
        return GaugeLed::Yellow;
    }
    return GaugeLed::Green;
}

/* Carburant : jaune sous la réserve (~10 % du réservoir), rouge sous le seuil du
 * voyant bas carburant. */
inline GaugeLed alarmeCarb(const HudData& d) noexcept {
    if (d.fuelLiters < physics::FUEL_LOW_L) {
        return GaugeLed::Red;
    }
    if (d.fuelLiters < physics::FUEL_CAUTION_L) {
        return GaugeLed::Yellow;
    }
    return GaugeLed::Green;
}

}  /* namespace artouste::ui::hud_widgets */
