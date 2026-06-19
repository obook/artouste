// Moteur audio simple (miniaudio) : boucles moteur et rotor dont le volume et
// la hauteur suivent l'état de vol. Optionnel : si le périphérique audio ou les
// fichiers sont absents, l'application reste silencieuse sans erreur.
//
// L'implémentation miniaudio (volumineuse) est cachée derrière un pImpl pour ne
// pas alourdir les autres unités de compilation.

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

    // Initialise le périphérique et charge les boucles depuis soundsDir.
    // Renvoie false (silencieux) en cas d'échec.
    bool init(const std::filesystem::path& soundsDir);

    // À appeler chaque image : module le son selon le collectif [0,1] et la
    // vitesse air (m/s).
    void update(float collective, float airspeed);

    [[nodiscard]] bool ready() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace artouste::audio
