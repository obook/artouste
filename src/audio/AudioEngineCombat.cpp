/*
 * AudioEngineCombat.cpp
 * Sons ponctuels du mode zombie (tir, touché, mort, jet, impact, nouvelle
 * vague) : chargement paresseux à la première lecture depuis
 * assets/sounds/combat/, même principe que AudioEngine::playMusic. Fichier
 * absent : silencieux, sans erreur.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "audio/AudioEngine.hpp"
#include "audio/AudioEngineImpl.hpp"

namespace artouste::audio {

namespace {

/* Charge (à la première lecture) puis (re)joue un son ponctuel depuis
   dir/filename. Fichier absent ou dossier jamais renseigné (initCombatSounds
   non appelée) : silencieux, sans erreur -- même contrat que playMusic. */
void playOneShot(ma_engine& engine, ma_sound& sound, bool& loaded,
                 const std::filesystem::path& dir, const char* filename, float volume) {
    if (dir.empty()) {
        return;
    }
    if (!loaded) {
        const std::filesystem::path path = dir / filename;
        if (!std::filesystem::exists(path)) {
            return;
        }
        if (ma_sound_init_from_file(&engine, path.string().c_str(), MA_SOUND_FLAG_STREAM, nullptr,
                                    nullptr, &sound) != MA_SUCCESS) {
            return;
        }
        loaded = true;
    }
    ma_sound_seek_to_pcm_frame(&sound, 0);
    ma_sound_set_volume(&sound, volume);
    ma_sound_start(&sound);
}

}  /* namespace */

void AudioEngine::initCombatSounds(const std::filesystem::path& dir) {
    m_impl->combatSoundsDir = dir;
}

void AudioEngine::playGunfire() {
    if (!m_impl->engineInit) {
        return;
    }
    playOneShot(m_impl->engine, m_impl->gunfireSound, m_impl->gunfireLoaded,
               m_impl->combatSoundsDir, "gunfire.wav", 0.35f);
}

void AudioEngine::playExplosion() {
    if (!m_impl->engineInit) {
        return;
    }
    playOneShot(m_impl->engine, m_impl->explosionSound, m_impl->explosionLoaded,
               m_impl->combatSoundsDir, "explosion.wav", 0.7f);
}

void AudioEngine::playZombieHit() {
    if (!m_impl->engineInit) {
        return;
    }
    playOneShot(m_impl->engine, m_impl->zombieHitSound, m_impl->zombieHitLoaded,
               m_impl->combatSoundsDir, "zombie_hit.wav", 0.6f);
}

void AudioEngine::playZombieDeath() {
    if (!m_impl->engineInit) {
        return;
    }
    playOneShot(m_impl->engine, m_impl->zombieDeathSound, m_impl->zombieDeathLoaded,
               m_impl->combatSoundsDir, "zombie_death.wav", 0.7f);
}

void AudioEngine::playToxicThrow() {
    if (!m_impl->engineInit) {
        return;
    }
    playOneShot(m_impl->engine, m_impl->toxicThrowSound, m_impl->toxicThrowLoaded,
               m_impl->combatSoundsDir, "toxic_throw.wav", 0.6f);
}

void AudioEngine::playToxicImpact() {
    if (!m_impl->engineInit) {
        return;
    }
    playOneShot(m_impl->engine, m_impl->toxicImpactSound, m_impl->toxicImpactLoaded,
               m_impl->combatSoundsDir, "toxic_impact.wav", 0.8f);
}

void AudioEngine::playWaveStart() {
    if (!m_impl->engineInit) {
        return;
    }
    playOneShot(m_impl->engine, m_impl->waveStartSound, m_impl->waveStartLoaded,
               m_impl->combatSoundsDir, "wave_start.wav", 0.7f);
}

}  /* namespace artouste::audio */
