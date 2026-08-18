/*
 * RadioStream.cpp
 * Implémentation du lecteur de flux radio internet (voir RadioStream.hpp).
 * Compile en deux variantes selon ARTOUSTE_HAS_CURL, réglé par CMakeLists.txt
 * quand libcurl est trouvée sur la machine de compilation (voir
 * cmake/Dependencies.cmake) :
 *   - définie : lecture réelle, flux MP3 récupéré par un thread réseau
 *     libcurl et décodé par miniaudio (voir plus bas, "ARTOUSTE_HAS_CURL défini") ;
 *   - non définie : coquille vide, toutes les méthodes no-op (radioPlaying()
 *     reste faux) -- le reste du moteur audio continue de fonctionner
 *     normalement, seule la radio est indisponible.
 * Un seul fichier plutôt que deux, car la garde de compilation EST le
 * mécanisme de sélection : il n'y a rien à partager entre les deux variantes.
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
bool RadioStream::playing() const {
    return false;
}

} /* namespace artouste::audio */

#else /* ARTOUSTE_HAS_CURL défini : lecture réelle -------------------------- */

#    include "audio/radio/RadioImpl.hpp"

#    include <chrono>
#    include <thread>

namespace artouste::audio {

RadioStream::RadioStream() : m_impl(std::make_unique<Impl>()) {}

RadioStream::~RadioStream() {
    stop();
    /* stop() ne joint plus (voir plus bas) : ici, en revanche, un join bloquant
       est nécessaire et sans danger -- l'objet disparaît, il n'y a plus d'image
       suivante pour absorber le coût en tâche de fond. */
    if (m_impl->netThread.joinable()) {
        m_impl->netThread.join();
    }
}

void RadioStream::start(ma_engine* engine, const std::string& url) {
    stop(); /* coupe un éventuel flux précédent */
    /* stop() ne joint plus l'ancien thread : on le fait ici avant d'en relancer un,
       sans quoi les deux tourneraient de concert sur le même Impl. Bloquant, mais
       sans danger : redémarrer la radio juste après l'avoir coupée est rare, et
       le thread a déjà eu tout le temps de finir depuis le stop() qui précède. */
    if (m_impl->netThread.joinable()) {
        m_impl->netThread.join();
        m_impl->netFinished.store(false);
    }
    if (engine == nullptr || url.empty()) {
        return;
    }
    m_impl->engine = engine;
    m_impl->url = url;
    m_impl->channels = ma_engine_get_channels(engine);
    m_impl->sampleRate = ma_engine_get_sample_rate(engine);
    m_impl->buffer.clear();
    m_impl->running.store(true);
    m_impl->netThread = std::thread([impl = m_impl.get()] { impl->netLoop(); });
}

void RadioStream::poll() {
    Impl* impl = m_impl.get();
    impl->reapNetThread(); /* absorbe ici le coût du join, hors du chemin de stop() */
    if (!impl->running.load() || impl->soundReady) {
        return; /* pas en marche, ou déjà prêt */
    }
    if (impl->buffer.available() < PRIME_BYTES) {
        return; /* pas encore assez d'octets pour lire l'en-tête MP3 */
    }

    /* Chaîne d'initialisation en trois étapes (décodeur, source de données,
       son) : à la première qui échoue, on défait ce qui a pu être construit
       et on abandonne en silence. */
    if (!impl->initDecoder() || !impl->initDataSource() || !impl->initSound()) {
        impl->teardownSound();
        impl->running.store(false);
        return;
    }

    ma_sound_set_volume(&impl->sound, impl->volume);
    ma_sound_start(&impl->sound);
}

void RadioStream::stop() {
    Impl* impl = m_impl.get();
    impl->running.store(false);
    impl->buffer.close(); /* débloque un write en attente */
    /* Le thread réseau n'est PAS joint ici : le join peut prendre de quelques ms
       à plusieurs centaines (curl n'avorte qu'à son prochain appel de progression,
       voir onCurlProgress), ce qui gelait l'image à chaque coupure de la radio.
       teardownSound(), lui, ne coûte rien à mesurer (miniaudio ne bloque pas sur
       l'arrêt d'un son) : rien à différer de ce côté. netThread sera rejoint sans
       bloquer par reapNetThread(), appelée depuis poll() ; start() et le
       destructeur, eux, joignent au besoin de façon bloquante (voir plus haut). */
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

} /* namespace artouste::audio */

#endif /* ARTOUSTE_HAS_CURL */
