/*
 * RadioSource.hpp
 * Source de données miniaudio du flux radio, et réglages du tampon.
 *
 * Le son ne se termine jamais : quand le décodeur n'a pas de quoi remplir la
 * demande, la source complète en silence.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "audio/RingBuffer.hpp"

#include <miniaudio.h>

#include <atomic>
#include <cstddef>

namespace artouste::audio {

constexpr std::size_t BUFFER_BYTES   = 256 * 1024; /* ~6 s à 320 kbps */
constexpr std::size_t PRIME_BYTES    = 48 * 1024;  /* octets à accumuler avant de décoder */
constexpr float       RADIO_VOLUME   = 0.45f;      /* sous les sons moteur */
constexpr int         RETRY_MS       = 1000;       /* attente avant reconnexion */
constexpr int         RETRY_SLICE_MS = 50;         /* granularité, pour rester réactif à l'arrêt */

/*
 * RadioCallbackCtx : données partagées entre Impl et les callbacks de la
 * source de données miniaudio. Défini ici (pas dans Impl) pour que les
 * fonctions libres puissent l'utiliser sans toucher au type privé Impl.
 */
struct RadioCallbackCtx {
    ma_decoder* pDecoder = nullptr;
    RingBuffer* pBuffer = nullptr;
    ma_uint32 channels = 2;
    /* Atomique : lu par le fil audio, écrit par le fil principal. */
    std::atomic<bool>* pDecoderReady = nullptr;
};

/*
 * RadioDataSource : enveloppe miniaudio custom.
 * ma_data_source_base DOIT être le premier champ (exigence de l'API miniaudio) :
 * le moteur audio caste le pointeur void* directement en ma_data_source_base*.
 * Le champ ctx donne accès aux ressources partagées avec les callbacks.
 */
struct RadioDataSource {
    ma_data_source_base base; /* premier champ obligatoire */
    RadioCallbackCtx ctx;
    ma_uint32 sampleRate = 48000;
};

/* Table des méthodes de la source, définie dans RadioSource.cpp. */
extern ma_data_source_vtable g_radioSourceVtable;

} /* namespace artouste::audio */
