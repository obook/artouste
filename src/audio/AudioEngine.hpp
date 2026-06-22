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

    /* Point d'écoute, selon la caméra active. Chaque vue a son propre rendu sonore
     * (mixage turbine/pales, timbre, Doppler) :
     *   - Interior : depuis la cabine, son plus étouffé et plus sombre ;
     *   - Rear     : vue de poursuite, équilibré et clair, juste derrière ;
     *   - Fly      : caméra libre extérieure, souffle rotor présent et effet Doppler. */
    enum class View { Interior, Rear, Fly };

    /* À appeler à chaque image : module le son selon le collectif [0, 1], la
     * vitesse air (en m/s), le régime turbine [0, 1] (sifflement de la turbine)
     * et le régime rotor [0, 1] (souffle des pales). 'view' choisit le rendu par
     * caméra ; 'closingSpeed' est la vitesse de rapprochement caméra<->appareil
     * (m/s, positive si la caméra se rapproche), pour l'effet Doppler. Au repos,
     * tout est silencieux ; au démarrage, la turbine siffle avant le rotor. */
    void update(float collective, float airspeed, float turbineFraction, float rotorFraction,
                View view, float closingSpeed);

    /* Suspend les boucles sonores (pause du jeu) ou les reprend. Idempotent :
     * n'agit qu'au changement d'état. */
    void setPaused(bool paused);

    /* Déclenche (depuis le début) le son ponctuel de démarrage de la turbine, et
     * l'arrête (par exemple si le pilote coupe la turbine en plein démarrage). */
    void playStartSound();
    void stopStartSound();

    /* Musique de la démo : lecture en boucle depuis le fichier donné (rejouée depuis
     * le début à chaque appel), et arrêt. Fichier absent : silencieux, sans erreur. */
    void playMusic(const std::filesystem::path& file);
    void stopMusic();

    [[nodiscard]] bool ready() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  /* namespace artouste::audio */
