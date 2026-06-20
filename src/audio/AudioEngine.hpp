/*
 * AudioEngine.hpp
 * Moteur audio simple (miniaudio) : des boucles moteur et rotor dont le volume
 * et la hauteur suivent l'état de vol. Tout est optionnel : sans périphérique
 * audio ni fichiers, l'application reste silencieuse, sans erreur.
 *
 * L'implémentation miniaudio, volumineuse, est cachée derrière un pImpl afin de
 * ne pas alourdir les autres fichiers qui incluent cet en-tête.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>
#include <memory>

namespace artouste::audio {

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&)            = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    /* Initialise le périphérique et charge les boucles depuis soundsDir.
     * Renvoie false, en silence, en cas d'échec. */
    bool init(const std::filesystem::path& soundsDir);

    /* À appeler à chaque image : module le son selon le collectif [0, 1] et la
     * vitesse air (en m/s). */
    void update(float collective, float airspeed);

    [[nodiscard]] bool ready() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  /* namespace artouste::audio */
