/*
 * AudioEngineImpl.hpp
 * Détails internes du moteur audio partagés par ses unités de compilation
 * (AudioEngine.cpp et AudioEngineControl.cpp) : la structure pImpl qui rassemble
 * les objets miniaudio, plus quelques utilitaires (bornage, rendu par vue, effet
 * Doppler). Cet en-tête inclut les déclarations de miniaudio, pas son
 * implémentation (générée une seule fois, dans AudioEngine.cpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "audio/AudioEngine.hpp"

#include <miniaudio.h>

namespace artouste::audio {

namespace audio_detail {

inline constexpr float START_VOLUME = 0.7f;  /* volume de base du son de démarrage */
inline constexpr float MUSIC_VOLUME = 0.5f;  /* volume de la musique de la démo (sous les sons moteur) */

inline float clamp01(float v) noexcept {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* Rendu sonore propre à une vue : volumes relatifs de la turbine et des pales
 * (le mixage), et un facteur de timbre (l'égalisation, approchée par la hauteur :
 * < 1 assombrit le son, comme étouffé dans la cabine). */
struct ViewMix {
    float turbineVol;
    float rotorVol;
    float tone;
};

inline ViewMix viewMix(AudioEngine::View view) noexcept {
    switch (view) {
        case AudioEngine::View::Interior:
            return {0.80f, 0.70f, 0.90f};  /* cabine : étouffé et plus sombre */
        case AudioEngine::View::Fly:
            return {0.90f, 1.00f, 1.00f};  /* extérieur : souffle des pales présent */
        case AudioEngine::View::Rear:
            break;
    }
    return {1.00f, 1.00f, 1.00f};  /* poursuite : équilibré et clair */
}

/* Décalage de hauteur dû à l'effet Doppler. closingSpeed > 0 = la caméra se
 * rapproche -> son plus aigu. On borne la vitesse pour éviter les extrêmes. */
inline float dopplerPitch(float closingSpeed) noexcept {
    constexpr float SOUND_SPEED = 340.0f;  /* m/s */
    const float v = closingSpeed < -120.0f ? -120.0f : (closingSpeed > 120.0f ? 120.0f : closingSpeed);
    return SOUND_SPEED / (SOUND_SPEED - v);
}

}  /* namespace audio_detail */

struct AudioEngine::Impl {
    ma_engine engine{};
    ma_sound  engineSound{};   /* boucle turbine, vue extérieure */
    ma_sound  rotorSound{};    /* boucle rotor,   vue extérieure */
    ma_sound  engineInside{};  /* boucle turbine, vue cabine */
    ma_sound  rotorInside{};   /* boucle rotor,   vue cabine */
    ma_sound  startSound{};    /* son ponctuel de démarrage turbine (non bouclé) */
    ma_sound  musicSound{};    /* musique de la démo (bouclée, chargée à la première lecture) */
    bool      engineInit         = false;
    bool      engineLoaded       = false;
    bool      rotorLoaded        = false;
    bool      engineInsideLoaded = false;
    bool      rotorInsideLoaded  = false;
    bool      startLoaded        = false;
    bool      musicLoaded        = false;
    bool      paused             = false;  /* boucles suspendues (pause du jeu) */
};

}  /* namespace artouste::audio */
