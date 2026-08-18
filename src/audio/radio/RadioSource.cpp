/*
 * RadioSource.cpp
 * Callbacks de la source de données custom (voir RadioSource.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "audio/radio/RadioSource.hpp"

#include <cstring>

namespace artouste::audio {

/* Lit des trames PCM du décodeur ; complète en silence si le décodeur n'a pas
   pu en fournir assez (sous-alimentation). Renvoie toujours frameCount :
   le son ne se termine jamais (flux vivant). */
static ma_result
radioSourceRead(ma_data_source* ds, void* frames, ma_uint64 frameCount, ma_uint64* framesRead) {
    auto* rds = reinterpret_cast<RadioDataSource*>(ds);
    ma_uint64 got = 0;
    if (*rds->ctx.pDecoderReady) {
        ma_data_source_read_pcm_frames(rds->ctx.pDecoder, frames, frameCount, &got);
    }
    if (got < frameCount) {
        auto* f = static_cast<float*>(frames);
        std::memset(f + got * rds->ctx.channels,
                    0,
                    static_cast<std::size_t>(frameCount - got) * rds->ctx.channels * sizeof(float));
    }
    if (framesRead != nullptr) {
        *framesRead = frameCount; /* toujours plein : flux vivant */
    }
    return MA_SUCCESS;
}

static ma_result radioSourceSeek(ma_data_source* /*ds*/, ma_uint64 /*frameIndex*/) {
    return MA_NOT_IMPLEMENTED;
}

static ma_result radioSourceGetFormat(ma_data_source* ds,
                                      ma_format* fmt,
                                      ma_uint32* ch,
                                      ma_uint32* sr,
                                      ma_channel* /*channelMap*/,
                                      std::size_t /*channelMapCap*/) {
    auto* rds = reinterpret_cast<RadioDataSource*>(ds);
    if (fmt != nullptr) {
        *fmt = ma_format_f32;
    }
    if (ch != nullptr) {
        *ch = rds->ctx.channels;
    }
    if (sr != nullptr) {
        *sr = rds->sampleRate;
    }
    return MA_SUCCESS;
}

/* Table des méthodes de la source de données custom. */
ma_data_source_vtable g_radioSourceVtable = {
    radioSourceRead,
    radioSourceSeek,
    radioSourceGetFormat,
    nullptr, /* onGetCursor */
    nullptr, /* onGetLength */
    nullptr, /* onSetLooping */
    0        /* flags */
};

} /* namespace artouste::audio */
