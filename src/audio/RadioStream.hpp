/*
 * RadioStream.hpp
 * Lecteur d'un flux radio internet (MP3 sur HTTP) : un thread réseau (libcurl)
 * remplit un tampon circulaire, une source de données miniaudio custom décode le
 * MP3 et complète en silence en cas de sous-alimentation, le tout branché sur
 * l'ma_engine fourni. Tout est optionnel : sans libcurl (ARTOUSTE_HAS_CURL non
 * défini), URL vide ou réseau coupé, le lecteur reste silencieux sans erreur.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <memory>
#include <string>

struct ma_engine;

namespace artouste::audio {

class RadioStream {
public:
    RadioStream();
    ~RadioStream();

    RadioStream(const RadioStream&)            = delete;
    RadioStream& operator=(const RadioStream&) = delete;

    /* Démarre le flux à l'URL donnée, branché sur engine. No-op si engine est nul,
       si url est vide, ou si libcurl est absent à la compilation. Un éventuel flux
       en cours est d'abord arrêté. */
    void start(ma_engine* engine, const std::string& url);

    /* Arrête proprement le flux (idempotent). */
    void stop();

    /* À appeler régulièrement (chaque image) : finalise l'initialisation du son
       dès qu'assez d'octets sont en tampon. No-op sinon. */
    void poll();

    /* Suspend ou reprend la lecture (suit la pause du jeu). */
    void setPaused(bool paused);

    /* Vrai si un flux est démarré (en cours de mise en route ou en lecture). */
    [[nodiscard]] bool playing() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  /* namespace artouste::audio */
