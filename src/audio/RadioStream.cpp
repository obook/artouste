/*
 * RadioStream.cpp
 * Implémentation du lecteur de flux radio internet (voir RadioStream.hpp).
 * Compile en deux variantes : avec ARTOUSTE_HAS_CURL (lecture réelle via libcurl
 * + miniaudio), ou sans (coquille vide, toutes les méthodes no-op).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "audio/RadioStream.hpp"

#ifndef ARTOUSTE_HAS_CURL

/* --- Variante sans libcurl : coquille vide, radio indisponible ------------- */
namespace artouste::audio {

struct RadioStream::Impl {};

RadioStream::RadioStream() = default;
RadioStream::~RadioStream() = default;
void RadioStream::start(ma_engine*, const std::string&) {}
void RadioStream::stop() {}
void RadioStream::poll() {}
void RadioStream::setPaused(bool) {}
void RadioStream::setVolume(float) {}
bool RadioStream::playing() const { return false; }

}  /* namespace artouste::audio */

#else  /* ARTOUSTE_HAS_CURL défini : lecture réelle -------------------------- */

#include "audio/RingBuffer.hpp"

#include <miniaudio.h>
#include <curl/curl.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

namespace artouste::audio {

namespace {
constexpr std::size_t BUFFER_BYTES = 256 * 1024;  /* ~6 s à 320 kbps */
constexpr std::size_t PRIME_BYTES  = 48 * 1024;   /* octets à accumuler avant de décoder */
constexpr float       RADIO_VOLUME = 0.45f;        /* sous les sons moteur */
constexpr int         RETRY_MS     = 1000;         /* attente avant reconnexion */
}  /* namespace */

/*
 * RadioCallbackCtx : données partagées entre Impl et les callbacks de la
 * source de données miniaudio. Défini ici (pas dans Impl) pour que les
 * fonctions libres puissent l'utiliser sans toucher au type privé Impl.
 */
struct RadioCallbackCtx {
    ma_decoder*  pDecoder     = nullptr;
    RingBuffer*  pBuffer      = nullptr;
    ma_uint32    channels     = 2;
    bool*        pDecoderReady = nullptr;
};

/*
 * RadioDataSource : enveloppe miniaudio custom.
 * ma_data_source_base DOIT être le premier champ (exigence de l'API miniaudio) :
 * le moteur audio caste le pointeur void* directement en ma_data_source_base*.
 * Le champ ctx donne accès aux ressources partagées avec les callbacks.
 */
struct RadioDataSource {
    ma_data_source_base  base;   /* premier champ obligatoire */
    RadioCallbackCtx     ctx;
    ma_uint32            sampleRate = 48000;
};

/* ==========================================================================
   Callbacks de la source de données custom.
   Fonctions libres ; elles accèdent aux ressources via RadioCallbackCtx.
   ========================================================================== */

/* Lit des trames PCM du décodeur ; complète en silence si le décodeur n'a pas
   pu en fournir assez (sous-alimentation). Renvoie toujours frameCount :
   le son ne se termine jamais (flux vivant). */
static ma_result radioSourceRead(ma_data_source* ds, void* frames,
                                 ma_uint64 frameCount, ma_uint64* framesRead) {
    auto* rds = reinterpret_cast<RadioDataSource*>(ds);
    ma_uint64 got = 0;
    if (*rds->ctx.pDecoderReady) {
        ma_data_source_read_pcm_frames(rds->ctx.pDecoder, frames, frameCount, &got);
    }
    if (got < frameCount) {
        auto* f = static_cast<float*>(frames);
        std::memset(
            f + got * rds->ctx.channels,
            0,
            static_cast<std::size_t>(frameCount - got) * rds->ctx.channels
                * sizeof(float));
    }
    if (framesRead != nullptr) {
        *framesRead = frameCount;  /* toujours plein : flux vivant */
    }
    return MA_SUCCESS;
}

static ma_result radioSourceSeek(ma_data_source* /*ds*/, ma_uint64 /*frameIndex*/) {
    return MA_NOT_IMPLEMENTED;
}

static ma_result radioSourceGetFormat(ma_data_source* ds, ma_format* fmt,
                                      ma_uint32* ch, ma_uint32* sr,
                                      ma_channel* /*channelMap*/,
                                      std::size_t /*channelMapCap*/) {
    auto* rds = reinterpret_cast<RadioDataSource*>(ds);
    if (fmt != nullptr) { *fmt = ma_format_f32; }
    if (ch  != nullptr) { *ch  = rds->ctx.channels; }
    if (sr  != nullptr) { *sr  = rds->sampleRate; }
    return MA_SUCCESS;
}

/* Table des méthodes de la source de données custom. */
static ma_data_source_vtable g_radioSourceVtable = {
    radioSourceRead,
    radioSourceSeek,
    radioSourceGetFormat,
    nullptr,  /* onGetCursor */
    nullptr,  /* onGetLength */
    nullptr,  /* onSetLooping */
    0         /* flags */
};

/* ========================================================================== */

struct RadioStream::Impl {
    ma_engine*               engine = nullptr;
    std::string              url;
    RingBuffer               buffer{BUFFER_BYTES};
    std::thread              netThread;
    std::atomic<bool>        running{false};  /* le thread réseau doit tourner */

    /* Côté lecture (créé dans poll() quand le tampon est amorcé). */
    ma_decoder               decoder{};
    RadioDataSource          radioDataSource{};
    ma_sound                 sound{};
    bool                     decoderReady    = false;
    bool                     dataSourceReady = false;
    bool                     soundReady      = false;
    ma_uint32                channels        = 2;
    ma_uint32                sampleRate      = 48000;
    float                    volume          = RADIO_VOLUME;  /* volume du flux [0, 1] (crossfade) */

    /* Callback d'écriture de libcurl : pousse les octets reçus dans le tampon.
       Bloque si le tampon est plein (régule le débit). Renvoie une taille
       différente de celle reçue pour faire avorter curl lors de l'arrêt. */
    static std::size_t onCurlWrite(char* ptr, std::size_t size, std::size_t nmemb,
                                   void* user) {
        auto* self        = static_cast<Impl*>(user);
        const std::size_t total = size * nmemb;
        if (!self->running.load()) {
            return 0;  /* avorte le transfert */
        }
        if (!self->buffer.write(reinterpret_cast<const unsigned char*>(ptr), total)) {
            return 0;  /* tampon fermé (arrêt) : avorte */
        }
        return total;
    }

    /* Boucle réseau : GET continu, reconnexion tant que running. */
    void netLoop() {
        CURL* curl = curl_easy_init();
        if (curl == nullptr) {
            return;
        }
        while (running.load()) {
            curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  &Impl::onCurlWrite);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA,      this);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL,       1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            /* Volontairement sans en-tête Icy-MetaData : on reçoit du MP3 pur. */
            curl_easy_perform(curl);  /* rend la main à la coupure ou à l'arrêt */
            if (!running.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_MS));
        }
        curl_easy_cleanup(curl);
    }

    /* Callback de lecture d'octets MP3, tiré du tampon sans bloquer.
       Jamais MA_AT_END : la source custom complète en silence si besoin. */
    static ma_result onDecoderRead(ma_decoder* dec, void* out, std::size_t toRead,
                                   std::size_t* bytesRead) {
        auto* self = static_cast<Impl*>(dec->pUserData);
        const std::size_t got =
            self->buffer.read(static_cast<unsigned char*>(out), toRead);
        if (bytesRead != nullptr) {
            *bytesRead = got;
        }
        return MA_SUCCESS;
    }

    static ma_result onDecoderSeek(ma_decoder* /*dec*/, ma_int64 /*byteOffset*/,
                                   ma_seek_origin /*origin*/) {
        return MA_NOT_IMPLEMENTED;  /* flux non rembobinable */
    }

    void teardownSound() {
        if (soundReady) {
            ma_sound_uninit(&sound);
            soundReady = false;
        }
        if (dataSourceReady) {
            ma_data_source_uninit(&radioDataSource.base);
            dataSourceReady = false;
        }
        if (decoderReady) {
            ma_decoder_uninit(&decoder);
            decoderReady = false;
        }
    }
};

RadioStream::RadioStream() : m_impl(std::make_unique<Impl>()) {}

RadioStream::~RadioStream() { stop(); }

void RadioStream::start(ma_engine* engine, const std::string& url) {
    stop();  /* coupe un éventuel flux précédent */
    if (engine == nullptr || url.empty()) {
        return;
    }
    m_impl->engine     = engine;
    m_impl->url        = url;
    m_impl->channels   = ma_engine_get_channels(engine);
    m_impl->sampleRate = ma_engine_get_sample_rate(engine);
    m_impl->buffer.clear();
    m_impl->running.store(true);
    m_impl->netThread = std::thread([impl = m_impl.get()] { impl->netLoop(); });
}

void RadioStream::poll() {
    Impl* impl = m_impl.get();
    if (!impl->running.load() || impl->soundReady) {
        return;  /* pas en marche, ou déjà prêt */
    }
    if (impl->buffer.available() < PRIME_BYTES) {
        return;  /* pas encore assez d'octets pour lire l'en-tête MP3 */
    }

    /* Décodeur MP3 lisant le tampon. */
    ma_decoder_config dcfg = ma_decoder_config_init(
        ma_format_f32, impl->channels, impl->sampleRate);
    dcfg.encodingFormat = ma_encoding_format_mp3;
    if (ma_decoder_init(&Impl::onDecoderRead, &Impl::onDecoderSeek, impl,
                        &dcfg, &impl->decoder) != MA_SUCCESS) {
        impl->running.store(false);  /* échec : abandon silencieux */
        return;
    }
    impl->decoderReady = true;

    /* Source de données custom (complète en silence) au-dessus du décodeur. */
    impl->radioDataSource.ctx.pDecoder      = &impl->decoder;
    impl->radioDataSource.ctx.pBuffer       = &impl->buffer;
    impl->radioDataSource.ctx.channels      = impl->channels;
    impl->radioDataSource.ctx.pDecoderReady = &impl->decoderReady;
    impl->radioDataSource.sampleRate        = impl->sampleRate;
    ma_data_source_config scfg              = ma_data_source_config_init();
    scfg.vtable                             = &g_radioSourceVtable;
    if (ma_data_source_init(&scfg, &impl->radioDataSource.base) != MA_SUCCESS) {
        impl->teardownSound();
        impl->running.store(false);
        return;
    }
    impl->dataSourceReady = true;

    /* Son branché sur l'ma_engine. */
    if (ma_sound_init_from_data_source(impl->engine, &impl->radioDataSource,
                                       0, nullptr, &impl->sound) != MA_SUCCESS) {
        impl->teardownSound();
        impl->running.store(false);
        return;
    }
    impl->soundReady = true;
    ma_sound_set_volume(&impl->sound, impl->volume);
    ma_sound_start(&impl->sound);
}

void RadioStream::stop() {
    Impl* impl = m_impl.get();
    impl->running.store(false);
    impl->buffer.close();          /* débloque un write en attente */
    if (impl->netThread.joinable()) {
        impl->netThread.join();
    }
    impl->teardownSound();
    impl->engine = nullptr;
}

void RadioStream::setPaused(bool paused) {
    Impl* impl = m_impl.get();
    if (!impl->soundReady) {
        return;
    }
    if (paused) {
        ma_sound_stop(&impl->sound);
    } else {
        ma_sound_start(&impl->sound);
    }
}

void RadioStream::setVolume(float volume) {
    Impl* impl = m_impl.get();
    impl->volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    if (impl->soundReady) {
        ma_sound_set_volume(&impl->sound, impl->volume);
    }
}

bool RadioStream::playing() const {
    return m_impl->running.load();
}

}  /* namespace artouste::audio */

#endif  /* ARTOUSTE_HAS_CURL */
