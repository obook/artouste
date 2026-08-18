/*
 * RadioLecture.cpp
 * Chaîne de lecture : décodeur MP3, source de données, son branché sur le
 * moteur audio.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "audio/radio/RadioImpl.hpp"

namespace artouste::audio {

ma_result RadioStream::Impl::onDecoderRead(ma_decoder* dec, void* out, std::size_t toRead,
                                          std::size_t* bytesRead) {
    auto* self = static_cast<Impl*>(dec->pUserData);
    const std::size_t got = self->buffer.read(static_cast<unsigned char*>(out), toRead);
    if (bytesRead != nullptr) {
        *bytesRead = got;
    }
    return MA_SUCCESS;
}

void RadioStream::Impl::teardownSound() {
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

bool RadioStream::Impl::initDecoder() {
    ma_decoder_config dcfg = ma_decoder_config_init(ma_format_f32, channels, sampleRate);
    dcfg.encodingFormat = ma_encoding_format_mp3;
    if (ma_decoder_init(&Impl::onDecoderRead, &Impl::onDecoderSeek, this, &dcfg, &decoder) !=
        MA_SUCCESS) {
        return false;
    }
    decoderReady = true;
    return true;
}

bool RadioStream::Impl::initDataSource() {
    radioDataSource.ctx.pDecoder = &decoder;
    radioDataSource.ctx.pBuffer = &buffer;
    radioDataSource.ctx.channels = channels;
    radioDataSource.ctx.pDecoderReady = &decoderReady;
    radioDataSource.sampleRate = sampleRate;
    ma_data_source_config scfg = ma_data_source_config_init();
    scfg.vtable = &g_radioSourceVtable;
    if (ma_data_source_init(&scfg, &radioDataSource.base) != MA_SUCCESS) {
        return false;
    }
    dataSourceReady = true;
    return true;
}

bool RadioStream::Impl::initSound() {
    if (ma_sound_init_from_data_source(engine, &radioDataSource, 0, nullptr, &sound) !=
        MA_SUCCESS) {
        return false;
    }
    soundReady = true;
    return true;
}

ma_result RadioStream::Impl::onDecoderSeek(ma_decoder* /*dec*/, ma_int64 /*byteOffset*/,
                                           ma_seek_origin /*origin*/) {
    return MA_NOT_IMPLEMENTED; /* flux non rembobinable */
}

} /* namespace artouste::audio */
