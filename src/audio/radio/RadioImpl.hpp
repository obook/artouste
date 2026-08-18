/*
 * RadioImpl.hpp
 * État interne du flux radio : tampon réseau, fil de téléchargement, chaîne de
 * lecture miniaudio.
 *
 * Déclaré à part pour que le fil réseau et la chaîne de lecture vivent dans
 * leurs propres fichiers.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "audio/RadioStream.hpp"

#include "audio/RingBuffer.hpp"
#include "audio/radio/RadioSource.hpp"

#include <curl/curl.h>
#include <miniaudio.h>

#include <atomic>
#include <cstddef>
#include <string>
#include <thread>

namespace artouste::audio {

struct RadioStream::Impl {
    ma_engine*  engine = nullptr;
    std::string url;
    RingBuffer  buffer{BUFFER_BYTES};
    std::thread netThread;

    std::atomic<bool> running{false}; /* le fil réseau doit tourner */
    /* Levé par netLoop() juste avant de rendre la main : signale qu'un join sur
       netThread ne bloquera plus (voir reapNetThread). */
    std::atomic<bool> netFinished{false};

    /* Côté lecture, créé dans poll() une fois le tampon amorcé. */
    ma_decoder      decoder{};
    RadioDataSource radioDataSource{};
    ma_sound        sound{};
    /* Lu par le fil audio de miniaudio (radioSourceRead), écrit par le fil
       principal (teardownSound) : atomique pour que ce partage soit défini. */
    std::atomic<bool> decoderReady{false};
    bool            dataSourceReady = false;
    bool            soundReady      = false;
    ma_uint32       channels        = 2;
    ma_uint32       sampleRate      = 48000;
    float           volume          = RADIO_VOLUME; /* volume du flux [0, 1] */

    /* --- Fil réseau, défini dans RadioReseau.cpp ---------------------------- */

    /* Pousse les octets reçus dans le tampon. Bloque si le tampon est plein, ce
       qui régule le débit. Renvoie une taille différente pour faire avorter curl
       à l'arrêt. */
    static std::size_t onCurlWrite(char* ptr, std::size_t size, std::size_t nmemb, void* user);

    /* Appelé même pendant la connexion : seul moyen d'avorter sans attendre de
       données quand l'arrêt tombe entre deux paquets. */
    static int onCurlProgress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t);

    /* GET continu, reconnexion tant que running. */
    void netLoop();

    /* Rejoint netThread s'il a fini, sans jamais bloquer sinon : appelé depuis
       poll() pour absorber le coût du join en tâche de fond plutôt qu'au stop(). */
    void reapNetThread();

    /* --- Chaîne de lecture, définie dans RadioLecture.cpp ------------------- */

    /* Octets MP3 tirés du tampon sans bloquer. Jamais MA_AT_END : la source
       custom complète en silence. */
    static ma_result onDecoderRead(ma_decoder* dec, void* out, std::size_t toRead,
                                   std::size_t* bytesRead);
    static ma_result onDecoderSeek(ma_decoder* dec, ma_int64 byteOffset, ma_seek_origin origin);

    void teardownSound();

    /* Les trois étages de poll() une fois le tampon amorcé, chacun au-dessus du
       précédent. Chacun lève son drapeau *Ready pour que teardownSound() sache
       quoi défaire en cas d'échec plus loin dans la chaîne. */
    bool initDecoder();
    bool initDataSource();
    bool initSound();
};

} /* namespace artouste::audio */
