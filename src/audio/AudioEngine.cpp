/*
 * AudioEngine.cpp
 * Moteur audio avec miniaudio : cycle de vie (construction, libération),
 * chargement des boucles moteur et rotor (init) et modulation continue de leur
 * volume et de leur hauteur selon l'état de vol (update). La pause, le son de
 * démarrage et la musique sont dans AudioEngineControl.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "audio/AudioEngine.hpp"

/* L'implémentation de miniaudio (volumineuse) n'est générée qu'ici, dans cette
   seule unité de compilation. Les autres fichiers audio n'incluent que les
   déclarations (via AudioEngineImpl.hpp). */
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "audio/AudioEngineImpl.hpp"

#include <cstdio>
#include <string>

namespace artouste::audio {

using namespace audio_detail;

AudioEngine::AudioEngine() : m_impl(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() {
    if (m_impl->engineLoaded) {
        ma_sound_uninit(&m_impl->engineSound);
    }
    if (m_impl->rotorLoaded) {
        ma_sound_uninit(&m_impl->rotorSound);
    }
    if (m_impl->engineInsideLoaded) {
        ma_sound_uninit(&m_impl->engineInside);
    }
    if (m_impl->rotorInsideLoaded) {
        ma_sound_uninit(&m_impl->rotorInside);
    }
    if (m_impl->startLoaded) {
        ma_sound_uninit(&m_impl->startSound);
    }
    if (m_impl->musicLoaded) {
        ma_sound_uninit(&m_impl->musicSound);
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

    const auto loadLoop = [&](ma_sound& sound, const char* file) -> bool {
        const std::filesystem::path path = soundsDir / file;
        if (!std::filesystem::exists(path)) {
            return false;
        }
        if (ma_sound_init_from_file(&m_impl->engine, path.string().c_str(),
                                    MA_SOUND_FLAG_STREAM, nullptr, nullptr, &sound) != MA_SUCCESS) {
            return false;
        }
        ma_sound_set_looping(&sound, MA_TRUE);
        /* On démarre la boucle silencieuse : turbine coupée au lancement, c'est
         * update() qui montera le volume une fois la turbine en route. Sans cela,
         * un court extrait se ferait entendre entre l'init et la première image. */
        ma_sound_set_volume(&sound, 0.0f);
        ma_sound_start(&sound);
        return true;
    };

    /* Son ponctuel (non bouclé) : on le charge sans le démarrer ; playStartSound()
     * le déclenchera au démarrage de la turbine. */
    const auto loadOneShot = [&](ma_sound& sound, const char* file) -> bool {
        const std::filesystem::path path = soundsDir / file;
        if (!std::filesystem::exists(path)) {
            return false;
        }
        return ma_sound_init_from_file(&m_impl->engine, path.string().c_str(),
                                       MA_SOUND_FLAG_STREAM, nullptr, nullptr, &sound) == MA_SUCCESS;
    };

    m_impl->engineLoaded       = loadLoop(m_impl->engineSound, "running-outside.wav");
    m_impl->rotorLoaded        = loadLoop(m_impl->rotorSound, "rotor-outside.wav");
    m_impl->engineInsideLoaded = loadLoop(m_impl->engineInside, "running-inside.wav");
    m_impl->rotorInsideLoaded  = loadLoop(m_impl->rotorInside, "rotor-inside.wav");
    m_impl->startLoaded        = loadOneShot(m_impl->startSound, "turbine-start.wav");

    if (!m_impl->engineLoaded && !m_impl->rotorLoaded) {
        std::printf("[audio] aucune boucle sonore trouvée, son désactivé.\n");
        return false;
    }
    std::printf("[audio] son actif.\n");
    return true;
}

void AudioEngine::update(float collective, float airspeed, float turbineFraction,
                         float rotorFraction, View view, float closingSpeed) {
    if (!m_impl->engineInit) {
        return;
    }
    const float   turbine  = clamp01(turbineFraction);  /* régime turbine [0, 1] */
    const float   rotor    = clamp01(rotorFraction);    /* régime rotor   [0, 1] */
    const ViewMix mix      = viewMix(view);
    const float   doppler  = dopplerPitch(closingSpeed);
    const bool    interior = (view == View::Interior);

    /* Aiguille une source (turbine ou rotor) vers sa boucle extérieure ou sa boucle
     * cabine, selon la vue. En cabine, on préfère la vraie boucle "inside" (déjà
     * étouffée à l'enregistrement) ; à défaut, on étouffe la boucle extérieure via
     * le facteur de timbre 'tone'. Le Doppler décale les deux de la même façon. */
    const auto routeSource = [&](ma_sound& outside, bool outLoaded, ma_sound& inside,
                                 bool inLoaded, float volume, float basePitch) {
        const bool useInside = interior && inLoaded;
        if (outLoaded) {
            const float muffle = (interior && !inLoaded) ? mix.tone : 1.0f;
            ma_sound_set_volume(&outside, useInside ? 0.0f : volume);
            ma_sound_set_pitch(&outside, basePitch * muffle * doppler);
        }
        if (inLoaded) {
            ma_sound_set_volume(&inside, useInside ? volume : 0.0f);
            ma_sound_set_pitch(&inside, basePitch * doppler);
        }
    };

    /* Turbine : volume et hauteur suivent son régime (silencieuse à l'arrêt, la
     * hauteur grimpe pendant la montée en régime). Le collectif l'accentue un peu. */
    const float c          = clamp01(collective);
    const float turbineVol = (0.35f + 0.55f * c) * turbine * mix.turbineVol;
    const float turbinePit = 0.55f + 0.35f * turbine + 0.30f * c;
    routeSource(m_impl->engineSound, m_impl->engineLoaded, m_impl->engineInside,
                m_impl->engineInsideLoaded, turbineVol, turbinePit);

    /* Rotor : ne se fait entendre qu'une fois les pales lancées, un peu plus quand
     * l'appareil avance (translation). Son volume suit le régime rotor. */
    const float a        = airspeed > 40.0f ? 1.0f : airspeed / 40.0f;
    const float rotorVol = (0.35f + 0.20f * a) * rotor * mix.rotorVol;
    routeSource(m_impl->rotorSound, m_impl->rotorLoaded, m_impl->rotorInside,
                m_impl->rotorInsideLoaded, rotorVol, 1.0f);

    /* Son de démarrage : ponctuel, mais il dure longtemps et domine pendant la
     * montée en régime. On le module donc aussi selon la vue (volume turbine et
     * étouffement en cabine), faute de quoi changer de vue au démarrage ne
     * s'entend pas. Il n'a pas de version cabine : on étouffe par la hauteur. */
    if (m_impl->startLoaded && ma_sound_is_playing(&m_impl->startSound) == MA_TRUE) {
        const float muffle = interior ? mix.tone : 1.0f;
        ma_sound_set_volume(&m_impl->startSound, START_VOLUME * mix.turbineVol);
        ma_sound_set_pitch(&m_impl->startSound, muffle * doppler);
    }
}

}  /* namespace artouste::audio */
