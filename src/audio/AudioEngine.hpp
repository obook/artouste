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

#include "audio/RadioStream.hpp"
#include "util/Math.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace artouste::audio {

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
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
    void update(float collective,
                float airspeed,
                float turbineFraction,
                float rotorFraction,
                View view,
                float closingSpeed);

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

    /* Mode zombie : mémorise le dossier des sons ponctuels (tir, touché, mort,
     * jet, impact, nouvelle vague), chargés paresseusement à leur première
     * lecture (comme playMusic). À appeler une fois à l'initialisation de la
     * scène (voir Application::initScene) ; les play* ci-dessous restent
     * silencieux, sans erreur, tant qu'aucun fichier n'est trouvé dans ce
     * dossier (assets/sounds/combat/ par défaut).
     *
     * Chaque appel (sauf playWaveStart) crée sa propre instance de lecture,
     * détruite automatiquement dès la fin de la lecture (voir reapOneShots dans
     * AudioEngine.cpp) : plusieurs sons identiques peuvent ainsi se superposer
     * (deux zombies tués la même image, par exemple) sans se couper la parole.
     * 'sourcePos' est la position monde de l'événement (explosion, zombie...),
     * 'listenerPos' celle de l'hélico ; le volume décroît avec leur distance.
     * playWaveStart() et playRale() sont les exceptions : annonces non
     * spatiales, à volume fixe, qui réutilisent chacune un unique son rejoué
     * depuis le début. Le largueur apparaît au bord de l'arène, à quelques
     * centaines de mètres : un râle spatialisé y serait inaudible, alors que
     * l'apparition du boss doit justement s'entendre. */
    void initCombatSounds(const std::filesystem::path& dir);
    void playGunfire(const vec3& sourcePos, const vec3& listenerPos);
    void playExplosion(const vec3& sourcePos, const vec3& listenerPos);
    void playZombieHit(const vec3& sourcePos, const vec3& listenerPos);
    /* Deux cris de mort distincts : le zombie qui ne laisse rien derrière
     * lui (Simple) et celui dont l'explosion a lancé une fusée de bonus
     * (Bonus). Le joueur entend donc à l'oreille, sans regarder le ciel,
     * si sa roquette lui a rapporté quelque chose. */
    void playZombieDeathSimple(const vec3& sourcePos, const vec3& listenerPos);
    void playZombieDeathBonus(const vec3& sourcePos, const vec3& listenerPos);
    void playToxicThrow(const vec3& sourcePos, const vec3& listenerPos);
    void playToxicImpact(const vec3& sourcePos, const vec3& listenerPos);
    void playDrink(const vec3& sourcePos, const vec3& listenerPos);
    void playSphereLaunch(const vec3& sourcePos, const vec3& listenerPos);
    void playSphereOpen(const vec3& sourcePos, const vec3& listenerPos);
    void playSphereSante(const vec3& sourcePos, const vec3& listenerPos);
    void playSphereMort(const vec3& sourcePos, const vec3& listenerPos);
    void playWaveStart();
    /* Râle du largueur : à son arrivée ET à sa mort. Même échantillon
     * (rale.wav), rejoué depuis le début -- les deux moments ne peuvent pas
     * tomber sur la même image, un largueur ne meurt pas en apparaissant. */
    void playRale();
    /* Coupe toutes les lectures de combat en cours (fin de partie) : setPaused
     * ne suspend que les boucles, et la purge des sons ponctuels s'arrête avec
     * update(). Sans effet s'il n'y a rien à couper. */
    void stopCombatSounds();

    /* Flux radio internet branché sur le moteur audio. URL vide ou libcurl absente :
     * no-op silencieux. startRadio (re)démarre le flux, stopRadio le coupe,
     * toggleRadio bascule (touche K), pollRadio finalise l'init du son une fois le
     * tampon amorcé (à appeler chaque image), radioPlaying indique si un flux tourne.
     * Définies dans AudioEngineRadio.cpp. */
    void startRadio(const std::string& url);
    void stopRadio();
    void toggleRadio(const std::string& url);
    void pollRadio();
    [[nodiscard]] bool radioPlaying() const;

    /* Crossfade radio/hélico : adjustRadioMix décale la balance de delta (vers la
     * radio si delta > 0, vers l'hélico si delta < 0), borné dans [0, 1]. radioMix
     * renvoie la part de la radio (0 = tout hélico, 1 = tout radio).
     * Définies dans AudioEngineRadio.cpp. */
    void adjustRadioMix(float delta);
    [[nodiscard]] float radioMix() const;

    /* Joue un message radio : 'text' (anglais) est synthétisé par Flite puis passé
     * dans un effet "radio". Aucun fichier audio. À accompagner d'un sous-titre côté
     * HUD. No-op silencieux si le périphérique audio ou le TTS est absent.
     * Définie dans AudioEngineRadio.cpp. */
    void playRadioMessage(const std::string& text);

    /* Vrai tant qu'un message radio est en cours de lecture. Sert à attendre la fin
     * de l'autorisation de la tour avant d'engager le rotor.
     * Définie dans AudioEngineRadio.cpp. */
    [[nodiscard]] bool radioMessagePlaying() const;

    [[nodiscard]] bool ready() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    RadioStream m_radio; /* lecteur du flux radio (pImpl autonome, sans miniaudio dans l'en-tête) */
};

} /* namespace artouste::audio */
