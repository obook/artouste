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

    /* À appeler à chaque image : module le son selon le collectif [0, 1], la
     * vitesse air (en m/s), le régime turbine [0, 1] (sifflement de la turbine)
     * et le régime rotor [0, 1] (souffle des pales). Au repos, tout est
     * silencieux ; au démarrage, la turbine siffle avant que le rotor ne tourne. */
    void update(float collective, float airspeed, float turbineFraction, float rotorFraction);

    [[nodiscard]] bool ready() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  /* namespace artouste::audio */
