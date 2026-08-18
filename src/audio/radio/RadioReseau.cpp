/*
 * RadioReseau.cpp
 * Fil de téléchargement du flux : GET continu, reconnexion, arrêt propre.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "audio/radio/RadioImpl.hpp"

#include <chrono>
#include <thread>

namespace artouste::audio {

std::size_t RadioStream::Impl::onCurlWrite(char* ptr, std::size_t size, std::size_t nmemb,
                                          void* user) {
    auto* self = static_cast<Impl*>(user);
    const std::size_t total = size * nmemb;
    if (!self->running.load()) {
        return 0; /* avorte le transfert */
    }
    if (!self->buffer.write(reinterpret_cast<const unsigned char*>(ptr), total)) {
        return 0; /* tampon fermé (arrêt) : avorte */
    }
    return total;
}

int RadioStream::Impl::onCurlProgress(void* user, curl_off_t, curl_off_t, curl_off_t,
                                     curl_off_t) {
    auto* self = static_cast<Impl*>(user);
    return self->running.load() ? 0 : 1;
}

void RadioStream::Impl::netLoop() {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        netFinished.store(true);
        return;
    }
    while (running.load()) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &Impl::onCurlWrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        /* Sans ce callback, un arrêt pendant la connexion ou entre deux
           paquets attend jusqu'à CURLOPT_CONNECTTIMEOUT ou le prochain
           paquet avant qu'onCurlWrite ne puisse avorter le transfert. */
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &Impl::onCurlProgress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
        /* Volontairement sans en-tête Icy-MetaData : on reçoit du MP3 pur. */
        curl_easy_perform(curl); /* rend la main à la coupure ou à l'arrêt */
        if (!running.load()) {
            break;
        }
        /* Attente découpée en tranches, plutôt qu'un seul sleep_for(RETRY_MS) :
           un arrêt demandé pendant l'attente de reconnexion doit être vu au
           prochain réveil, pas après jusqu'à une seconde entière. */
        for (int attendu = 0; attendu < RETRY_MS && running.load(); attendu += RETRY_SLICE_MS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_SLICE_MS));
        }
    }
    curl_easy_cleanup(curl);
    netFinished.store(true);
}

void RadioStream::Impl::reapNetThread() {
    if (netThread.joinable() && netFinished.load()) {
        netThread.join();
        netFinished.store(false);
    }
}

} /* namespace artouste::audio */
