/*
 * AudioEngine.cpp
 * Implémentation du moteur audio avec miniaudio : chargement des boucles
 * moteur et rotor, puis modulation de leur volume et de leur hauteur selon
 * le collectif et la vitesse air.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "audio/AudioEngine.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <cstdio>
#include <string>

namespace artouste::audio {

namespace {

float clamp01(float v) noexcept {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

}  /* namespace */

struct AudioEngine::Impl {
    ma_engine engine{};
    ma_sound  engineSound{};
    ma_sound  rotorSound{};
    bool      engineInit   = false;
    bool      engineLoaded = false;
    bool      rotorLoaded  = false;
};

AudioEngine::AudioEngine() : m_impl(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() {
    if (m_impl->engineLoaded) {
        ma_sound_uninit(&m_impl->engineSound);
    }
    if (m_impl->rotorLoaded) {
        ma_sound_uninit(&m_impl->rotorSound);
    }
    if (m_impl->engineInit) {
        ma_engine_uninit(&m_impl->engine);
    }
}

bool AudioEngine::init(const std::filesystem::path& soundsDir) {
    if (ma_engine_init(nullptr, &m_impl->engine) != MA_SUCCESS) {
        std::fprintf(stderr, "[audio] périphérique indisponible, son désactivé.\n");
        return false;
    }
    m_impl->engineInit = true;

    const auto loadLoop = [&](ma_sound& sound, const char* file, float volume) -> bool {
        const std::filesystem::path path = soundsDir / file;
        if (!std::filesystem::exists(path)) {
            return false;
        }
        if (ma_sound_init_from_file(&m_impl->engine, path.string().c_str(),
                                    MA_SOUND_FLAG_STREAM, nullptr, nullptr, &sound) != MA_SUCCESS) {
            return false;
        }
        ma_sound_set_looping(&sound, MA_TRUE);
        ma_sound_set_volume(&sound, volume);
        ma_sound_start(&sound);
        return true;
    };

    m_impl->engineLoaded = loadLoop(m_impl->engineSound, "running-outside.wav", 0.5f);
    m_impl->rotorLoaded  = loadLoop(m_impl->rotorSound, "rotor-outside.wav", 0.4f);

    if (!m_impl->engineLoaded && !m_impl->rotorLoaded) {
        std::printf("[audio] aucune boucle sonore trouvée, son désactivé.\n");
        return false;
    }
    std::printf("[audio] son actif.\n");
    return true;
}

void AudioEngine::update(float collective, float airspeed, float turbineFraction,
                         float rotorFraction) {
    if (!m_impl->engineInit) {
        return;
    }
    const float turbine = clamp01(turbineFraction);  /* régime turbine [0, 1] */
    const float rotor   = clamp01(rotorFraction);    /* régime rotor   [0, 1] */

    /* Turbine : volume et hauteur suivent son régime (silencieuse à l'arrêt, la
     * hauteur grimpe pendant la montée en régime : sifflement de démarrage). Le
     * collectif l'accentue un peu (la turbine pousse plus fort en charge). */
    if (m_impl->engineLoaded) {
        const float c = clamp01(collective);
        ma_sound_set_volume(&m_impl->engineSound, (0.35f + 0.55f * c) * turbine);
        ma_sound_set_pitch(&m_impl->engineSound, 0.55f + 0.35f * turbine + 0.30f * c);
    }
    /* Rotor : ne se fait entendre qu'une fois les pales lancées, un peu plus
     * quand l'appareil avance (translation). Son volume suit le régime rotor. */
    if (m_impl->rotorLoaded) {
        const float a = airspeed > 40.0f ? 1.0f : airspeed / 40.0f;
        ma_sound_set_volume(&m_impl->rotorSound, (0.35f + 0.20f * a) * rotor);
    }
}

bool AudioEngine::ready() const noexcept {
    return m_impl->engineInit && (m_impl->engineLoaded || m_impl->rotorLoaded);
}

}  /* namespace artouste::audio */
