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

#include <list>
#include <vector>

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

/* Atténuation des sons ponctuels du mode zombie selon la distance à l'hélico
 * (voir AudioEngine::playExplosion et consorts) : plein volume jusqu'à
 * FULL_VOLUME_DIST_M, silence au-delà de SILENT_DIST_M, transition adoucie
 * entre les deux (plus agréable qu'une chute linéaire). */
inline constexpr float SOUND_FULL_VOLUME_DIST_M = 20.0f;
inline constexpr float SOUND_SILENT_DIST_M       = 250.0f;

inline float distanceAttenuation(const vec3& sourcePos, const vec3& listenerPos) noexcept {
    const float dist = glm::length(sourcePos - listenerPos);
    const float t    = clamp01((dist - SOUND_FULL_VOLUME_DIST_M) /
                             (SOUND_SILENT_DIST_M - SOUND_FULL_VOLUME_DIST_M));
    const float smooth = t * t * (3.0f - 2.0f * t);
    return 1.0f - smooth;
}

/* Instance de lecture ponctuelle en cours, avec le modèle dont elle est issue :
   c'est lui qui sert à compter les occurrences simultanées d'un MÊME son, pour
   les plafonner (voir playPositional). Sans ce plafond, une horde entière
   lançant ses pneus empilait une dizaine d'exemplaires d'un échantillon de sept
   secondes, dont la somme saturait la sortie et noyait tout le reste. */
struct OneShot {
    ma_sound        sound{};
    const ma_sound* source = nullptr;  /* modèle d'origine, jamais déréférencé */
};

}  /* namespace audio_detail */

/* Purge les instances de lecture ponctuelles arrivées à leur fin (voir
   AudioEngineCombat.cpp) : appelée chaque image depuis AudioEngine::update. */
void reapOneShots(std::list<audio_detail::OneShot>& oneShots);

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
    float     radioMix           = 0.45f;  /* crossfade radio/hélico : 0 = tout hélico, 1 = tout radio */

    /* Sons ponctuels du mode zombie (tir, touché, mort, jet, impact, nouvelle
       vague), chargés paresseusement à la première lecture depuis
       combatSoundsDir (voir AudioEngine::initCombatSounds), même principe que
       musicSound. combatSoundsDir vide (init jamais appelée) : silencieux.
       Chacun (sauf waveStartSound) sert de MODÈLE, entièrement décodé
       (MA_SOUND_FLAG_DECODE) et jamais joué lui-même : chaque lecture en copie
       une nouvelle instance à la volée (ma_sound_init_copy, peu coûteux car les
       données décodées sont partagées) dans oneShots, pour que plusieurs
       occurrences du même son se superposent au lieu de s'interrompre. */
    std::filesystem::path combatSoundsDir;
    ma_sound  gunfireSound{};
    ma_sound  explosionSound{};
    ma_sound  zombieHitSound{};
    ma_sound  zombieDeathSimpleSound{};
    ma_sound  zombieDeathBonusSound{};
    ma_sound  toxicThrowSound{};
    ma_sound  toxicImpactSound{};
    ma_sound  drinkSound{};
    ma_sound  launchSphereSound{};
    ma_sound  sphereOpenSound{};
    ma_sound  sphereSanteSound{};
    ma_sound  sphereMortSound{};
    /* Ces deux-là sont les exceptions : rejoués depuis le début, jamais copiés
       (voir playWaveStart et playRale). */
    ma_sound  waveStartSound{};
    ma_sound  broodSpawnSound{};
    bool      gunfireLoaded     = false;
    bool      explosionLoaded   = false;
    bool      zombieHitLoaded   = false;
    bool      zombieDeathSimpleLoaded = false;
    bool      zombieDeathBonusLoaded  = false;
    bool      toxicThrowLoaded  = false;
    bool      toxicImpactLoaded = false;
    bool      drinkLoaded       = false;
    bool      launchSphereLoaded  = false;
    bool      sphereOpenLoaded    = false;
    bool      sphereSanteLoaded = false;
    bool      sphereMortLoaded  = false;
    bool      waveStartLoaded   = false;
    bool      broodSpawnLoaded  = false;

    /* Instances de lecture en cours, une par appel à playGunfire/playExplosion/
       etc. std::list : contrairement à un vector, il ne déplace jamais les
       éléments existants (une réallocation invaliderait les ma_sound déjà
       démarrés). Purgées à chaque image (voir reapOneShots, appelé depuis
       update()) dès que ma_sound_at_end() les signale terminées. */
    std::list<audio_detail::OneShot> oneShots;

    /* Message radio : voix de synthèse (Flite) "radioïsée", générée à la volée sans
       fichier. Le tampon PCM doit rester en vie tant que la source l'utilise : ici. */
    std::vector<float> msgData;             /* PCM mono du message en cours */
    ma_audio_buffer    msgBuffer{};         /* source de données sur msgData */
    ma_sound           msgSound{};          /* lecture one-shot du message */
    bool               msgBufferReady = false;
    bool               msgSoundReady  = false;
    float              msgDuck        = 1.0f;  /* gain hélico abaissé pendant un message (lissé) */
};

}  /* namespace artouste::audio */
