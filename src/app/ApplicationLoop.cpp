/*
 * ApplicationLoop.cpp
 * Boucle principale du simulateur et ses étapes : calcul des commandes, caméra,
 * audio et animation du rotor. mainLoop enchaîne ces étapes à chaque image, avec
 * une physique intégrée à pas fixe (240 Hz) indépendante de la cadence de rendu.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <GLFW/glfw3.h>

#include "app/AppConstants.hpp"
#include "app/Clock.hpp"
#include "input/InputSystem.hpp"
#include "physics/RigidBody.hpp"
#include "render/Camera.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app {

namespace {

/*
 * Rotor principal (animation visuelle) :
 *   - ROTOR_SPIN_RATE : vitesse de rotation à plein régime, en rad/s. Volontairement
 *     ralentie par rapport à la réalité pour éviter l'effet stroboscopique.
 *   - BLADE_SPACING : écart entre deux pales (rotor tripale -> 120 degrés). Les
 *     positions de parking sont les multiples de cet écart : une pale alignée sur
 *     l'axe de l'appareil, les deux autres encadrant la sortie d'échappement.
 *   - PARK_TAU : constante de temps du retour en position de parking, à l'arrêt.
 */
constexpr float ROTOR_SPIN_RATE = 16.0f;
constexpr float BLADE_SPACING   = 2.0944f;  /* 2*pi/3 rad ~ 120 degrés */
constexpr float PARK_TAU        = 0.6f;

}  /* namespace */

physics::Controls Application::computeControls(const physics::Controls& rawInput, float frameDt) {
    /* Mode démo : une vraie action du pilote (manche, palonnier ou collectif)
     * lui rend la main et coupe la démo. */
    if (m_demo.active()) {
        const float pilotInput = std::fabs(rawInput.cyclicLateral)
                               + std::fabs(rawInput.cyclicLongitudinal)
                               + std::fabs(rawInput.pedals)
                               + std::fabs(rawInput.collective);
        if (pilotInput > 0.15f) {
            m_demo.stop();
        }
    }

    /* Commandes effectives : fournies par le pilote automatique en mode démo,
     * sinon les commandes du pilote passées par le mode assisté (qui les corrige
     * et les adoucit s'il est actif, sinon les laisse intactes). */
    physics::Controls controls;
    if (m_demo.active()) {
        const physics::RigidBody& demoBody    = m_flight.body();
        const vec3                demoFwd      = demoBody.orientation * vec3{1.0f, 0.0f, 0.0f};
        const float               demoHeading  = std::atan2(-demoFwd.z, demoFwd.x);
        const float demoGround = m_terrain->heightAt(demoBody.position.x, demoBody.position.z);
        const DemoPilot::Output demoOut =
            m_demo.update(frameDt, demoBody.position, demoBody.velocity, demoHeading, demoGround,
                          m_flight.turbine().rotorFraction());
        controls = demoOut.controls;
        /* Fin de vol : la démo demande de couper la turbine une fois posée. */
        if (demoOut.cutTurbine) {
            m_flight.turbine().toggle();
        }
        /* La démo impose la vue et le HUD, sauf si l'utilisateur a repris la main
           dessus (touches C / H pendant la démo) : on respecte alors son choix. */
        if (!m_demoUserView) {
            m_viewMode = demoOut.viewMode;
        }
        /* HUD de la démo : il change à chaque cycle de vues (aucun, complet
           superposé, puis quatre coins). Les étiquettes des lieux restent
           affichées même sans HUD (voir le rendu du HUD). */
        if (!m_demoUserHud) {
            switch (demoOut.hudStyle) {
                case 1:  m_hudMode = ui::HudMode::Overlay; break;  /* complet (Super HUD) */
                case 2:  m_hudMode = ui::HudMode::Corners; break;  /* quatre coins */
                default: m_hudMode = ui::HudMode::Off;     break;  /* aucun */
            }
        }
        if (demoOut.finished) {
            startDemo();  /* la démo est terminée : on la rejoue en boucle */
        }
    } else {
        controls = m_assist.apply(rawInput, frameDt);
    }
    return controls;
}

void Application::updateCamera(const mat4& base, const vec3& renderPos, float yaw, float t,
                               float frameDt) {
    /* Caméra : poursuite, cockpit (solidaire de l'appareil) ou orbite libre.
       Le changement de vue est un cut net : on remet à zéro le lissage de la
       poursuite pour qu'elle se cale instantanément, sans glissement. */
    if (m_viewMode != m_prevCamView) {
        if (m_prevCamView >= 0) {
            m_camera.cut();
        }
        if (m_viewMode == 2) {
            m_orbitStart = t;  /* début d'un segment orbite (pour le tour complet en démo) */
        }
        m_prevCamView = m_viewMode;
    }

    const vec3 lookTarget = renderPos + vec3{0.0f, 1.2f, 0.0f};
    if (m_viewMode == 1) {  /* cockpit */
        const vec3 eye = vec3(base * vec4(COCKPIT_EYE, 1.0f));
        const vec3 fwd = mat3(base) * glm::normalize(vec3{1.0f, -0.22f, 0.0f});
        const vec3 up  = mat3(base) * vec3{0.0f, 1.0f, 0.0f};
        m_camera.setFovYDeg(70.0f);
        m_camera.setNear(0.05f);  /* petit : ne tranche pas la verrière toute proche */
        m_camera.setLookAt(eye, eye + fwd, up);
    } else if (m_viewMode == 2) {  /* orbite */
        m_camera.setFovYDeg(60.0f);
        m_camera.setNear(0.5f);
        /* En démo, la caméra fait un tour complet (360 deg) autour de l'appareil
           sur la durée du segment orbite ; en pilotage manuel, rotation lente
           continue. (DEMO_ORBIT_TURN doit valoir la durée du segment orbite de la
           démo, voir DemoPilot.) */
        constexpr float DEMO_ORBIT_TURN = 14.0f;  /* s pour un tour complet en démo */
        const float angle = m_demo.active()
                                ? (t - m_orbitStart) * (TWO_PI / DEMO_ORBIT_TURN)
                                : t * 0.25f;
        m_camera.orbit(lookTarget, 15.0f, 6.0f, angle);
    }
    else if (m_viewMode == 3)
    {  /* orbite solaire */
        m_camera.setFovYDeg(60.0f);
        m_camera.setNear(0.5f);
        
        /* Même soleil que l'éclairage et le ciel (source unique). */
        const vec3 sunDir = sunDirection(t);
        m_camera.orbitSolar(lookTarget, sunDir, 15.0f, 6.0f);
    }
    else {  /* poursuite */
        m_camera.setFovYDeg(60.0f);
        m_camera.setNear(0.5f);
        m_camera.chase(lookTarget, yaw, frameDt);
    }
}

void Application::updateAudio(const physics::RigidBody& body, const physics::Controls& controls,
                              float airspeed, float turbineFraction, float rotorFraction,
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
        m_viewMode == 1 ? audio::AudioEngine::View::Interior   /* cockpit */
        : (m_viewMode == 2 || m_viewMode == 3) ? audio::AudioEngine::View::Fly /* orbite et orbite solaire */
                          : audio::AudioEngine::View::Rear;       /* poursuite */

    /* Effet Doppler : uniquement en vue extérieure libre (orbite). On le déduit
     * de la vitesse propre de l'appareil projetée sur l'axe caméra->appareil (la
     * caméra d'orbite est quasi fixe), et non d'une différence de distance entre
     * deux images : ainsi un changement de vue ou un reset, qui téléporte la
     * caméra, ne crée aucun Doppler parasite (la vitesse reste bornée). En vues
     * intérieure et poursuite, la caméra suit l'appareil -> pas de mouvement
     * relatif, pas de Doppler. Un léger lissage adoucit l'entrée/sortie d'effet. */
    float targetClosing = 0.0f;
    if (audioView == audio::AudioEngine::View::Fly) {
        const vec3  toCam = m_camera.position() - body.position;
        const float dist  = glm::length(toCam);
        if (dist > 0.001f) {
            targetClosing = glm::dot(body.velocity, toCam / dist);
        }
    }
    m_closingSpeed += (targetClosing - m_closingSpeed) * (1.0f - std::exp(-frameDt / 0.25f));

    /* En pause, on suspend les boucles sonores ; sinon on les module normalement. */
    m_audio.setPaused(m_paused);
    if (!m_paused) {
        m_audio.update(controls.collective, airspeed, turbineFraction, rotorFraction,
                       audioView, m_closingSpeed);
    }
    /* Finalise l'init du son de la radio dès que le tampon réseau est amorcé. */
    m_audio.pollRadio();
}

void Application::advanceRotor(float rotorFraction, float frameDt) {
    /* Le rotor n'avance qu'au prorata du régime (donc pales immobiles turbine
     * coupée, puis accélération), dans le sens horaire vu de dessus (angle
     * décroissant). À l'arrêt, on le ramène en douceur à la position de parking la
     * plus proche : une pale presque dans l'axe de l'appareil (à un léger décalage
     * aléatoire près, voir m_parkOffset), les deux autres de part et d'autre de la
     * sortie de la turbine, pour qu'aucune ne stationne dans le jet chaud. Figé en
     * pause, comme le reste de la simulation. */
    if (m_paused) {
        return;
    }
    if (rotorFraction > 0.0f) {
        m_rotorAngle -= rotorFraction * ROTOR_SPIN_RATE * frameDt;
    } else {
        /* Multiple de 120° le plus proche, décalé du jitter de parking, pour
         * que la pale ne soit pas figée pile dans l'axe. */
        const float park =
            std::round((m_rotorAngle - m_parkOffset) / BLADE_SPACING) * BLADE_SPACING
            + m_parkOffset;
        const float ease = 1.0f - std::exp(-frameDt / PARK_TAU);
        m_rotorAngle += (park - m_rotorAngle) * ease;
    }
}

void Application::mainLoop() {
    constexpr float SIM_DT = 1.0f / 240.0f;  /* pas fixe de simulation (240 Hz) */

    Clock clock;
    float accumulator = 0.0f;

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE) {
        glfwPollEvents();
        clock.tick();
        const float t = static_cast<float>(clock.elapsed());
        /* On borne le pas de temps : cela évite un saut brutal à la première
         * image ou après un gel (débogage, fenêtre déplacée). */
        const float frameDt = clamp(static_cast<float>(clock.dt()), 0.0f, 0.1f);

        /* Entrées -> commandes effectives (pilote automatique en démo, sinon
         * pilote humain adouci par le mode assisté), puis boutons d'action. */
        const physics::Controls rawInput = m_input->poll(frameDt);
        const physics::Controls controls = computeControls(rawInput, frameDt);

        /* Musique de la démo : coupée dès que la démo s'arrête (entrée pilote,
           touche V, etc.). Le lancement, lui, se fait dans startDemo(). */
        const bool demoActiveNow = m_demo.active();
        if (m_demoWasActive && !demoActiveNow) {
            m_audio.stopMusic();
        }
        m_demoWasActive = demoActiveNow;

        handleActionButtons();

        /* Hauteur du relief sous l'appareil : sert au contact avec le sol. */
        const vec3& pos = m_flight.body().position;
        m_flight.setGroundHeight(m_terrain->heightAt(pos.x, pos.z));

        /* État physique avant le dernier pas, conservé pour interpoler le rendu. */
        physics::RigidBody prevBody = m_flight.body();
        if (m_paused || m_confirmReset || m_confirmDemo) {  /* un panneau de confirmation fige le vol */
            accumulator = 0.0f;  /* pas de rattrapage à la reprise */
        } else {
            accumulator += frameDt;
            while (accumulator >= SIM_DT) {
                prevBody = m_flight.body();
                m_flight.update(controls, SIM_DT);
                accumulator -= SIM_DT;
            }
        }

        const physics::RigidBody& body = m_flight.body();

        /* Interpolation entre l'avant-dernier et le dernier état physique, selon le
         * reste de l'accumulateur. La physique tourne à pas fixe (240 Hz) et le rendu
         * à une autre cadence : sans cette interpolation, l'appareil avance d'un
         * nombre variable de pas par image, ce qui se voit comme un sautillement,
         * surtout en vue de poursuite. */
        const float alpha     = accumulator / SIM_DT;
        const vec3  renderPos = glm::mix(prevBody.position, body.position, alpha);
        const quat  renderOri = glm::slerp(prevBody.orientation, body.orientation, alpha);

        /* Transformation monde de l'appareil : translation + orientation (quaternion). */
        const mat4 base = glm::translate(mat4(1.0f), renderPos) * glm::mat4_cast(renderOri);

        /* Cap (lacet) extrait de l'orientation, utile pour la caméra de poursuite. */
        const vec3  forward = renderOri * vec3{1.0f, 0.0f, 0.0f};
        const float yaw     = std::atan2(-forward.z, forward.x);

        updateCamera(base, renderPos, yaw, t, frameDt);

        const float airspeed        = glm::length(vec2{body.velocity.x, body.velocity.z});
        const float turbineFraction = m_flight.turbine().turbineFraction();
        const float rotorFraction   = m_flight.turbine().rotorFraction();

        updateAudio(body, controls, airspeed, turbineFraction, rotorFraction, frameDt);
        advanceRotor(rotorFraction, frameDt);

        renderScene(base, m_rotorAngle, rotorFraction, controls.pedals,
                    controls.cyclicLongitudinal, controls.cyclicLateral, controls.collective,
                    turbineFraction, t);

        ui::HudData hud;
        fillHud(hud, body, forward, controls, airspeed, turbineFraction, rotorFraction);
        buildNavHud(hud, body.position, hud.headingDeg);
        /* En démo, le HUD est éteint mais on garde les étiquettes des lieux. */
        m_hud.render(hud, m_hudMode, m_paused, m_confirmReset, m_confirmDemo, m_demo.active());

        glfwSwapBuffers(m_window);
    }
}

}  /* namespace artouste::app */
