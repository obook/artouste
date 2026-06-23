/*
 * AudioEngineControl.cpp
 * Commandes ponctuelles du moteur audio : suspension/reprise des boucles (pause),
 * son de démarrage de la turbine, musique de la démo, et état "prêt". Le cycle de
 * vie et la modulation continue sont dans AudioEngine.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "audio/AudioEngine.hpp"

#include "audio/AudioEngineImpl.hpp"

#include <string>

namespace artouste::audio {

using namespace audio_detail;

void AudioEngine::setPaused(bool paused) {
    if (!m_impl->engineInit || paused == m_impl->paused) {
        return;  /* audio inactif, ou état déjà à jour : rien à faire */
    }
    m_impl->paused = paused;

    /* Suspend ou reprend chaque boucle chargée. ma_sound_stop garde la position,
     * ma_sound_start reprend là où on s'était arrêté. */
    const auto applyTo = [paused](ma_sound& sound, bool loaded) {
        if (!loaded) {
            return;
        }
        if (paused) {
            ma_sound_stop(&sound);
        } else {
            ma_sound_start(&sound);
        }
    };
    applyTo(m_impl->engineSound, m_impl->engineLoaded);
    applyTo(m_impl->rotorSound, m_impl->rotorLoaded);
    applyTo(m_impl->engineInside, m_impl->engineInsideLoaded);
    applyTo(m_impl->rotorInside, m_impl->rotorInsideLoaded);
    /* Le son de démarrage est ponctuel : on le coupe en pause, sans le reprendre
     * automatiquement (il ne doit pas rejouer tout seul à la reprise). */
    if (paused && m_impl->startLoaded) {
        ma_sound_stop(&m_impl->startSound);
    }
    /* La radio suit la pause du jeu (suspend ou reprend le flux). */
    m_radio.setPaused(paused);
}

void AudioEngine::playStartSound() {
    if (!m_impl->engineInit || !m_impl->startLoaded) {
        return;
    }
    /* Rejoue le son de démarrage depuis le début (déclenché à l'entrée en phase
     * de démarrage de la turbine). */
    ma_sound_seek_to_pcm_frame(&m_impl->startSound, 0);
    ma_sound_set_volume(&m_impl->startSound, START_VOLUME);
    ma_sound_start(&m_impl->startSound);
}

void AudioEngine::stopStartSound() {
    if (!m_impl->engineInit || !m_impl->startLoaded) {
        return;
    }
    ma_sound_stop(&m_impl->startSound);
}

void AudioEngine::playMusic(const std::filesystem::path& file) {
    if (!m_impl->engineInit) {
        return;
    }
    /* Chargement paresseux à la première lecture (en streaming, pour un long titre).
       Fichier absent ou illisible : on reste silencieux, sans erreur. */
    if (!m_impl->musicLoaded) {
        if (!std::filesystem::exists(file)) {
            return;
        }
        if (ma_sound_init_from_file(&m_impl->engine, file.string().c_str(),
                                    MA_SOUND_FLAG_STREAM, nullptr, nullptr,
                                    &m_impl->musicSound) != MA_SUCCESS) {
            return;
        }
        ma_sound_set_looping(&m_impl->musicSound, MA_TRUE);
        m_impl->musicLoaded = true;
    }
    /* (Re)lance la musique depuis le début. */
    ma_sound_seek_to_pcm_frame(&m_impl->musicSound, 0);
    ma_sound_set_volume(&m_impl->musicSound, MUSIC_VOLUME);
    ma_sound_start(&m_impl->musicSound);
}

void AudioEngine::stopMusic() {
    if (m_impl->engineInit && m_impl->musicLoaded) {
        ma_sound_stop(&m_impl->musicSound);
    }
}

void AudioEngine::startRadio(const std::string& url) {
    if (!m_impl->engineInit) {
        return;  /* pas de périphérique audio : radio silencieuse */
    }
    m_radio.setVolume(m_impl->radioMix);  /* applique la balance courante */
    m_radio.start(&m_impl->engine, url);
}

void AudioEngine::stopRadio() {
    m_radio.stop();
}

void AudioEngine::toggleRadio(const std::string& url) {
    if (m_radio.playing()) {
        m_radio.stop();
    } else {
        startRadio(url);
    }
}

void AudioEngine::pollRadio() {
    m_radio.poll();
}

bool AudioEngine::radioPlaying() const {
    return m_radio.playing();
}

void AudioEngine::adjustRadioMix(float delta) {
    float m = m_impl->radioMix + delta;
    m = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
    m_impl->radioMix = m;
    m_radio.setVolume(m);  /* le gain hélico (1 - m) est appliqué dans update() */
}

float AudioEngine::radioMix() const {
    return m_impl->radioMix;
}

bool AudioEngine::ready() const noexcept {
    return m_impl->engineInit && (m_impl->engineLoaded || m_impl->rotorLoaded);
}

}  /* namespace artouste::audio */
