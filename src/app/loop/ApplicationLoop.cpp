/*
 * ApplicationLoop.cpp
 * Boucle principale du simulateur et ses étapes : calcul des commandes, caméra
 * et audio. mainLoop enchaîne ces étapes à chaque image, avec une physique
 * intégrée à pas fixe (240 Hz) indépendante de la cadence de rendu.
 * L'animation du rotor et le message radio, autre étape par image, vivent dans
 * ApplicationRotorRadio.cpp.
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

physics::Controls Application::computeControls(const physics::Controls& rawInput, float frameDt,
                                               float t) {
    /* Mode démo : une vraie action du pilote (manche, palonnier ou collectif)
     * lui rend la main et coupe la démo. Un court délai de grâce après le lancement
     * (m_demoInputGraceS) ignore cette détection : sans lui, un résidu d'entrée -- par
     * exemple la touche D encore relâchée juste après avoir servi à lancer la démo
     * depuis le menu, D étant aussi le palonnier droit en vol -- coupe la démo dans les
     * toutes premières images, avant que le pilote n'ait rien demandé. */
    if (m_demo.active()) {
        if (m_demoInputGraceS > 0.0f) {
            m_demoInputGraceS -= frameDt;
        } else {
            const float pilotInput = std::fabs(rawInput.cyclicLateral)
                                   + std::fabs(rawInput.cyclicLongitudinal)
                                   + std::fabs(rawInput.pedals)
                                   + std::fabs(rawInput.collective);
            if (pilotInput > 0.15f) {
                m_demo.stop();
                if (m_demoFromMenu) {
                    m_returnToMenu = true;  /* démo lancée depuis le menu : on y retourne */
                }
            }
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
                          m_flight.turbine().rotorFraction(), sunDirection(t).y);
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

    /* Par défaut, pas de tremblement : seules les vues externes le laissent à zéro,
       la vue cockpit le réactive ci-dessous selon le régime rotor. */
    m_camera.setShake(vec3{0.0f});

    const vec3 lookTarget = renderPos + vec3{0.0f, 1.2f, 0.0f};
    if (m_viewMode == 1) {  /* cockpit */
        const vec3 eye = vec3(base * vec4(COCKPIT_EYE, 1.0f));
        const vec3 fwd = mat3(base) * glm::normalize(vec3{1.0f, -0.22f, 0.0f});
        const vec3 up  = mat3(base) * vec3{0.0f, 1.0f, 0.0f};

        /* Vibrations rotor : trois impulsions par tour (3/rev, ~18 Hz à 360 tr/min)
         * font légèrement trembler la cabine. On décale l'oeil dans le plan caméra,
         * d'autant plus que le rotor tourne vite. Effet visuel, pas une force.
         * Ce décalage est transmis comme tremblement appliqué en espace vue (voir
         * Camera::view) : toute l'image tremble alors d'un bloc, paysage compris, au
         * lieu d'être noyé dans les grandes coordonnées monde (le terrain restait
         * alors seul immobile). */
        const float rotorFraction = m_flight.turbine().rotorFraction();
        if (rotorFraction > 0.1f) {
            const vec3  side  = glm::normalize(glm::cross(fwd, up));
            const float freq  = 3.0f * rotorFraction * 360.0f / 60.0f;  /* Hz */
            const float phase = t * freq * TWO_PI;
            const float amp   = COCKPIT_VIBRATION_AMPLITUDE * rotorFraction;
            m_camera.setShake(side * (amp * std::sin(phase))
                              + up * (amp * 0.5f * std::sin(phase * 2.0f)));
        }

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
        constexpr float DEMO_ORBIT_TURN = 20.0f;  /* s pour un tour complet en démo */
        constexpr float ORBIT_SPEED = 0.12f;  /* rad/s en pilotage manuel (rotation lente) */
        const float     angle       = m_demo.active()
                                          ? (t - m_orbitStart) * (TWO_PI / DEMO_ORBIT_TURN)
                                          : t * ORBIT_SPEED;
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

bool Application::mainLoop() {
    constexpr float SIM_DT = 1.0f / 240.0f;  /* pas fixe de simulation (240 Hz) */

    Clock clock;
    float accumulator = 0.0f;

    m_returnToMenu = false;  /* on entre en vol : la demande de retour au menu est vierge */

    /* Amorçage des fronts de la manette : le menu se valide avec le bouton A, qui sert
       aussi à défiler la livrée en vol. Sans cela, le A de validation encore tenu à
       l'entrée en vol (retour au menu -> vol, sans rechargement) déclencherait un
       défilement de livrée dès la première image (olive -> rouge). Idem pour les autres
       boutons partagés avec le menu (X, Y). */
    if (m_input) {
        m_input->primeButtons();
    }

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE && !m_returnToMenu) {
        glfwPollEvents();
        clock.tick();
        /* On borne le pas de temps : cela évite un saut brutal à la première
         * image ou après un gel (débogage, fenêtre déplacée). */
        const float frameDt = clamp(static_cast<float>(clock.dt()), 0.0f, 0.1f);

        /* Tout est figé en pause comme pendant un panneau de confirmation : la
         * physique, mais aussi la démo, la caméra d'orbite et la vibration cockpit
         * (qui suivent m_animTime, lequel n'avance pas tant qu'on est figé). */
        const bool frozen = m_paused || m_confirmReset || m_confirmDemo;
        if (!frozen) {
            m_animTime += frameDt;
        }
        const float t = m_animTime;

        /* Entrées -> commandes effectives (pilote automatique en démo, sinon
         * pilote humain adouci par le mode assisté), puis boutons d'action. En
         * pause, on n'avance pas la démo : on garde les dernières commandes pour
         * que les gouvernes et le HUD ne reviennent pas au neutre. */
        const physics::Controls rawInput = m_input->poll(frameDt);
        physics::Controls controls = m_lastControls;
        if (!frozen) {
            controls       = computeControls(rawInput, frameDt, t);
            m_lastControls = controls;
        }

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

        /* Difficultés de pilotage (altitude, VNE, vol latéral, vortex ring state) :
           coupées en mode assisté et en démo, pour garder un vol facile et un parcours
           de démonstration prévisible jusqu'à 2000 m. */
        m_flight.setRealFlyPhysicsEnabled(!m_assist.active() && !m_demo.active());

        /* État physique avant le dernier pas, conservé pour interpoler le rendu. */
        physics::RigidBody prevBody = m_flight.body();
        if (frozen) {  /* pause ou panneau de confirmation : le vol est figé */
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

        /* Message radio : armé à la turbine au plein régime, émis 2 s après.
           Figé en pause, comme le reste. */
        if (!frozen) {
            updateRadioMessage(turbineFraction, frameDt);
        }

        updateAudio(body, controls, airspeed, turbineFraction, rotorFraction, frameDt);
        advanceRotor(rotorFraction, frameDt);

        renderScene(base, m_rotorAngle, rotorFraction, controls.pedals,
                    controls.cyclicLongitudinal, controls.cyclicLateral, controls.collective,
                    turbineFraction, t);

        ui::HudData hud;
        fillHud(hud, body, forward, controls, airspeed, turbineFraction, rotorFraction, t, frameDt);
        buildNavHud(hud, body.position, hud.headingDeg, t);
        /* Sous-titre du message radio simulé, tant que son temps d'affichage court. */
        hud.radioMessage = (m_radioMsgShow > 0.0f) ? m_radioMsg.c_str() : "";
        /* En démo, le HUD est éteint mais on garde les étiquettes des lieux. */
        m_hud.updateScale(m_width, m_height);  /* échelle et police, avant le NewFrame ImGui */
        m_hud.render(hud, m_hudMode, m_paused, m_confirmReset, m_confirmDemo, m_demo.active());

        glfwSwapBuffers(m_window);
    }

    return m_returnToMenu;  /* true = retour au menu ; false = fenêtre fermée (on quitte) */
}

}  /* namespace artouste::app */
