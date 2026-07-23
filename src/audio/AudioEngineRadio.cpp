/*
 * AudioEngineRadio.cpp
 * Radio internet et messages radio de synthèse vocale : filtre "voix radio"
 * (radioize), synthèse Flite (synthFlite), flux internet (startRadio et
 * consorts) et lecture d'un message (playRadioMessage). Le cycle de vie et la
 * modulation continue sont dans AudioEngine.cpp ; les commandes ponctuelles
 * hors radio (pause, son de démarrage, musique) sont dans
 * AudioEngineControl.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

/* Sous Windows, les en-têtes Flite (cst_file.h, cst_alloc.h) tirent <windows.h>.
 * WIN32_LEAN_AND_MEAN et NOMINMAX évitent que ses macros min/max n'entrent en
 * conflit avec std::max utilisé plus bas (même garde que ApplicationScene.cpp). */
#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#endif

#include "audio/AudioEngine.hpp"
#include "audio/AudioEngineImpl.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#ifdef ARTOUSTE_HAS_FLITE
/* Flite est en C : en-tête inclus en liaison C, et déclaration de l'enregistrement
   de la voix clustergen cmu_us_slt (voix naturelle). */
extern "C" {
#    include <flite.h>
}
extern "C" cst_voice* register_cmu_us_slt(const char* voxdir);
#endif

namespace artouste::audio {

using namespace audio_detail;

namespace {

/* Applique l'effet "radio" à un signal PCM mono (modifié sur place) : passe-bande
 * comms (300-3000 Hz), saturation douce, squelch d'ouverture/fermeture, "roger beep"
 * final, puis normalisation. Fait sonner une voix de synthèse comme une transmission. */
void radioize(std::vector<float>& s, unsigned int sampleRate) {
    const std::size_t N = s.size();
    if (N == 0) {
        return;
    }
    const float fs = static_cast<float>(sampleRate);
    constexpr float PI_F = 3.14159265358979f;

    /* Petit bruit reproductible pour le squelch (LCG). */
    std::uint32_t rng = static_cast<std::uint32_t>(N) | 1u;
    const auto rnd = [&rng]() -> float {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float>(rng >> 8) / static_cast<float>(1u << 24); /* [0, 1) */
    };

    /* Passe-haut ~300 Hz, passe-bas ~3000 Hz (bande comms), saturation douce. */
    {
        const float dt = 1.0f / fs;
        const float rcHp = 1.0f / (2.0f * PI_F * 300.0f);
        const float aHp = rcHp / (rcHp + dt);
        const float rcLp = 1.0f / (2.0f * PI_F * 3000.0f);
        const float aLp = dt / (rcLp + dt);
        float prevIn = 0.0f, prevHp = 0.0f, lp = 0.0f;
        for (std::size_t n = 0; n < N; ++n) {
            const float x = s[n];
            const float hp = aHp * (prevHp + x - prevIn);
            prevIn = x;
            prevHp = hp;
            lp += aLp * (hp - lp);
            s[n] = std::tanh(2.0f * lp);
        }
    }

    /* Squelch d'ouverture/fermeture + roger beep (~1000 Hz). */
    {
        const std::size_t burst = std::min<std::size_t>(N, static_cast<std::size_t>(0.05f * fs));
        for (std::size_t n = 0; n < burst; ++n) {
            const float a = 1.0f - static_cast<float>(n) / static_cast<float>(burst);
            s[n] += 0.4f * a * (rnd() * 2.0f - 1.0f);
            s[N - 1 - n] += 0.3f * a * (rnd() * 2.0f - 1.0f);
        }
        const std::size_t beep = std::min<std::size_t>(N, static_cast<std::size_t>(0.09f * fs));
        for (std::size_t n = 0; n < beep; ++n) {
            const std::size_t idx = N - beep + n;
            const float a = std::sin(PI_F * static_cast<float>(n) / static_cast<float>(beep));
            s[idx] += 0.3f * a * std::sin(2.0f * PI_F * 1000.0f * static_cast<float>(n) / fs);
        }
    }

    /* Normalisation à un pic élevé (la voix doit ressortir des sons hélico). */
    float peak = 1e-6f;
    for (const float v : s) {
        peak = std::max(peak, std::fabs(v));
    }
    const float norm = 0.92f / peak;
    for (float& v : s) {
        v *= norm;
    }
}

#ifdef ARTOUSTE_HAS_FLITE
/* Synthétise 'text' (anglais) avec la voix naturelle cmu_us_slt et renvoie le PCM
 * mono flottant, en plaçant sa fréquence dans 'sampleRate'. Flite n'est initialisé
 * qu'une fois. Renvoie un vecteur vide en cas d'échec. */
std::vector<float> synthFlite(const std::string& text, unsigned int& sampleRate) {
    static bool inited = false;
    static cst_voice* voice = nullptr;
    if (!inited) {
        flite_init();
        voice = register_cmu_us_slt(nullptr);
        if (voice != nullptr) {
            /* Débit plus rapide (style radio) : duration_stretch < 1 accélère la voix. */
            feat_set_float(voice->features, "duration_stretch", 0.82f);
        }
        inited = true;
    }
    std::vector<float> pcm;
    if (voice == nullptr) {
        return pcm;
    }
    cst_wave* w = flite_text_to_wave(text.c_str(), voice);
    if (w == nullptr) {
        return pcm;
    }
    sampleRate = static_cast<unsigned int>(w->sample_rate);
    const int num = w->num_samples;
    if (num > 0) {
        pcm.resize(static_cast<std::size_t>(num));
        for (int i = 0; i < num; ++i) {
            pcm[static_cast<std::size_t>(i)] = static_cast<float>(w->samples[i]) / 32768.0f;
        }
    }
    delete_wave(w);
    return pcm;
}
#endif

} /* namespace */

void AudioEngine::startRadio(const std::string& url) {
    if (!m_impl->engineInit) {
        return; /* pas de périphérique audio : radio silencieuse */
    }
    m_radio.setVolume(m_impl->radioMix); /* applique la balance courante */
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
    m_radio.setVolume(m); /* le gain hélico (1 - m) est appliqué dans update() */
}

float AudioEngine::radioMix() const {
    return m_impl->radioMix;
}

void AudioEngine::playRadioMessage(const std::string& text) {
    if (!m_impl->engineInit) {
        return;
    }
    /* Un seul message à la fois : on libère le précédent (le son puis sa source). */
    if (m_impl->msgSoundReady) {
        ma_sound_uninit(&m_impl->msgSound);
        m_impl->msgSoundReady = false;
    }
    if (m_impl->msgBufferReady) {
        ma_audio_buffer_uninit(&m_impl->msgBuffer);
        m_impl->msgBufferReady = false;
    }

#ifdef ARTOUSTE_HAS_FLITE
    unsigned int srcRate = 16000;
    m_impl->msgData = synthFlite(text, srcRate);
    if (m_impl->msgData.empty()) {
        return; /* synthèse indisponible : le sous-titre reste affiché, sans voix */
    }
    radioize(m_impl->msgData, srcRate);

    /* Tampon PCM mono à la fréquence de la voix ; l'ma_engine rééchantillonne et
       convertit vers le périphérique. ma_audio_buffer_init référence les données
       (sans copie) : msgData reste en vie dans Impl. */
    ma_audio_buffer_config cfg = ma_audio_buffer_config_init(
        ma_format_f32, 1, m_impl->msgData.size(), m_impl->msgData.data(), nullptr);
    cfg.sampleRate = srcRate;
    if (ma_audio_buffer_init(&cfg, &m_impl->msgBuffer) != MA_SUCCESS) {
        return;
    }
    m_impl->msgBufferReady = true;
    if (ma_sound_init_from_data_source(
            &m_impl->engine, &m_impl->msgBuffer, 0, nullptr, &m_impl->msgSound) != MA_SUCCESS) {
        ma_audio_buffer_uninit(&m_impl->msgBuffer);
        m_impl->msgBufferReady = false;
        return;
    }
    m_impl->msgSoundReady = true;
    /* Volume > 1 : on amplifie la voix (les sons hélico sont par ailleurs atténués
       pendant le message, voir le ducking dans update). */
    ma_sound_set_volume(&m_impl->msgSound, 1.6f);
    ma_sound_start(&m_impl->msgSound);
#else
    (void) text; /* TTS non compilé : sous-titre seul */
#endif
}

bool AudioEngine::radioMessagePlaying() const {
    return m_impl->msgSoundReady && ma_sound_is_playing(&m_impl->msgSound) == MA_TRUE;
}

} /* namespace artouste::audio */
