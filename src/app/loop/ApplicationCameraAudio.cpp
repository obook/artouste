/*
 * ApplicationCameraAudio.cpp
 * Place la caméra selon la vue courante (poursuite, cockpit, orbite ou orbite
 * solaire) et module les boucles sonores (rotor, turbine, effet Doppler).
 * Complète ApplicationLoop.cpp (mainLoop, qui appelle ces deux étapes à
 * chaque image) et ApplicationControls.cpp (commandes effectives).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/AppConstants.hpp"
#include "app/Application.hpp"
#include "render/Camera.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app {

void Application::updateCamera(
    const mat4& base, const vec3& renderPos, float yaw, float t, float frameDt) {
    /* Caméra : poursuite, cockpit (solidaire de l'appareil) ou orbite libre.
       Le changement de vue est un cut net : on remet à zéro le lissage de la
       poursuite pour qu'elle se cale instantanément, sans glissement. */
    if (m_viewMode != m_prevCamView) {
        if (m_prevCamView >= 0) {
            m_camera.cut();
        }
        if (m_viewMode == 2) {
            m_orbitStart = t; /* début d'un segment orbite (pour le tour complet en démo) */
        }
        m_prevCamView = m_viewMode;
    }

    /* Par défaut, pas de tremblement : seules les vues externes le laissent à zéro,
       la vue cockpit le réactive ci-dessous selon le régime rotor. */
    m_camera.setShake(vec3{0.0f});

    const vec3 lookTarget = renderPos + vec3{0.0f, 1.2f, 0.0f};
    if (m_viewMode == 1) { /* cockpit */
        const vec3 eye = vec3(base * vec4(COCKPIT_EYE, 1.0f));
        const vec3 fwd = mat3(base) * glm::normalize(vec3{1.0f, -0.22f, 0.0f});
        const vec3 up = mat3(base) * vec3{0.0f, 1.0f, 0.0f};

        /* Vibrations rotor : trois impulsions par tour (3/rev, ~18 Hz à 360 tr/min)
         * font légèrement trembler la cabine. On décale l'oeil dans le plan caméra,
         * d'autant plus que le rotor tourne vite. Effet visuel, pas une force.
         * Ce décalage est transmis comme tremblement appliqué en espace vue (voir
         * Camera::view) : toute l'image tremble alors d'un bloc, paysage compris, au
         * lieu d'être noyé dans les grandes coordonnées monde (le terrain restait
         * alors seul immobile). */
        const float rotorFraction = m_flight.turbine().rotorFraction();
        if (rotorFraction > 0.1f) {
            const vec3 side = glm::normalize(glm::cross(fwd, up));
            const float freq = 3.0f * rotorFraction * 360.0f / 60.0f; /* Hz */
            const float phase = t * freq * TWO_PI;
            const float amp = COCKPIT_VIBRATION_AMPLITUDE * rotorFraction;
            m_camera.setShake(side * (amp * std::sin(phase)) +
                              up * (amp * 0.5f * std::sin(phase * 2.0f)));
        }

        m_camera.setFovYDeg(70.0f);
        m_camera.setNear(0.05f); /* petit : ne tranche pas la verrière toute proche */
        m_camera.setLookAt(eye, eye + fwd, up);
    } else if (m_viewMode == 2) { /* orbite */
        m_camera.setFovYDeg(60.0f);
        m_camera.setNear(0.5f);
        /* En démo, la caméra fait un tour complet (360 deg) autour de l'appareil
           sur la durée du segment orbite ; en pilotage manuel, rotation lente
           continue. (DEMO_ORBIT_TURN doit valoir la durée du segment orbite de la
           démo, voir DemoPilot.) */
        constexpr float DEMO_ORBIT_TURN = 20.0f; /* s pour un tour complet en démo */
        constexpr float ORBIT_SPEED = 0.12f;     /* rad/s en pilotage manuel (rotation lente) */
        const float angle =
            m_demo.active() ? (t - m_orbitStart) * (TWO_PI / DEMO_ORBIT_TURN) : t * ORBIT_SPEED;
        m_camera.orbit(lookTarget, 15.0f, 6.0f, angle);
    } else if (m_viewMode == 3) { /* orbite solaire */
        m_camera.setFovYDeg(60.0f);
        m_camera.setNear(0.5f);

        /* Même soleil que l'éclairage et le ciel (source unique). */
        const vec3 sunDir = sunDirection(t);
        m_camera.orbitSolar(lookTarget, sunDir, 15.0f, 6.0f);
    } else { /* poursuite */
        m_camera.setFovYDeg(60.0f);
        m_camera.setNear(0.5f);
        m_camera.chase(lookTarget, yaw, frameDt);
    }
}

void Application::updateAudio(const physics::RigidBody& body,
                              const physics::Controls& controls,
                              float airspeed,
                              float turbineFraction,
                              float rotorFraction,
                              float frameDt) {
    /* Son ponctuel de démarrage : déclenché quand la turbine entre en phase de
     * démarrage, coupé si le pilote l'interrompt (passage en extinction). */
    const physics::Turbine::State turbineState = m_flight.turbine().state();
    if (turbineState != m_prevTurbineState) {
        if (turbineState == physics::Turbine::State::Demarrage) {
            m_audio.playStartSound();
        } else if (turbineState == physics::Turbine::State::Extinction) {
            m_audio.stopStartSound();
        }
        m_prevTurbineState = turbineState;
    }

    const audio::AudioEngine::View audioView =
        m_viewMode == 1 ? audio::AudioEngine::View::Interior /* cockpit */
        : (m_viewMode == 2 || m_viewMode == 3)
            ? audio::AudioEngine::View::Fly   /* orbite et orbite solaire */
            : audio::AudioEngine::View::Rear; /* poursuite */

    /* Effet Doppler : uniquement en vue extérieure libre (orbite). On le déduit
     * de la vitesse propre de l'appareil projetée sur l'axe caméra->appareil (la
     * caméra d'orbite est quasi fixe), et non d'une différence de distance entre
     * deux images : ainsi un changement de vue ou un reset, qui téléporte la
     * caméra, ne crée aucun Doppler parasite (la vitesse reste bornée). En vues
     * intérieure et poursuite, la caméra suit l'appareil -> pas de mouvement
     * relatif, pas de Doppler. Un léger lissage adoucit l'entrée/sortie d'effet. */
    float targetClosing = 0.0f;
    if (audioView == audio::AudioEngine::View::Fly) {
        const vec3 toCam = m_camera.position() - body.position;
        const float dist = glm::length(toCam);
        if (dist > 0.001f) {
            targetClosing = glm::dot(body.velocity, toCam / dist);
        }
    }
    m_closingSpeed += (targetClosing - m_closingSpeed) * (1.0f - std::exp(-frameDt / 0.25f));

    /* En pause, on suspend les boucles sonores ; sinon on les module normalement.
     * La fin de partie du mode zombie fige le vol comme une pause (voir 'frozen'
     * dans mainLoop) : le son se tait de la même façon, sans quoi turbine et
     * rotor continuent de tourner sur un appareil abattu, derrière le bandeau de
     * fin de partie. La reprise est automatique : setPaused garde la position des
     * boucles, et la partie suivante les relance là où elles s'étaient tues. */
    const bool audioFrozen = m_paused || m_combat.gameOver();
    m_audio.setPaused(audioFrozen);
    if (m_combat.gameOver()) {
        /* Les sons ponctuels du combat ne passent pas par setPaused : on les
           coupe à part, à chaque image de fin de partie (sans effet une fois la
           liste vide), plutôt que de guetter le front de gameOver. */
        m_audio.stopCombatSounds();
    }
    if (!audioFrozen) {
        m_audio.update(controls.collective,
                       airspeed,
                       turbineFraction,
                       rotorFraction,
                       audioView,
                       m_closingSpeed);
    }
    /* Finalise l'init du son de la radio dès que le tampon réseau est amorcé. */
    m_audio.pollRadio();
}

} /* namespace artouste::app */
