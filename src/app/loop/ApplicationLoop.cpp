/*
 * ApplicationLoop.cpp
 * Boucle principale du simulateur : mainLoop intègre la physique à pas fixe
 * (240 Hz), indépendante de la cadence de rendu, et enchaîne les étapes de
 * chaque image. Le calcul des commandes vit dans ApplicationControls.cpp, la
 * caméra et l'audio dans ApplicationCameraAudio.cpp, l'animation du rotor et
 * le message radio dans ApplicationRotorRadio.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "app/Clock.hpp"
#include "input/InputSystem.hpp"
#include "physics/RigidBody.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"
#include "util/Math.hpp"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace artouste::app {

bool Application::mainLoop() {
    constexpr float SIM_DT = 1.0f / 240.0f; /* pas fixe de simulation (240 Hz) */

    Clock clock;
    float accumulator = 0.0f;

    m_returnToMenu = false; /* on entre en vol : la demande de retour au menu est vierge */

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

        /* Journal de cadence (diagnostic) : ARTOUSTE_FPS_LOG=1 imprime les images
         * par seconde sur la sortie d'erreur, une fois par seconde. Sert à
         * distinguer un plafond vsync d'un vrai manque de performance GPU. */
        static const bool logFps = std::getenv("ARTOUSTE_FPS_LOG") != nullptr;
        if (logFps) {
            static int frames = 0;
            static double last = glfwGetTime();
            ++frames;
            const double now = glfwGetTime();
            if (now - last >= 1.0) {
                std::fprintf(stderr, "[fps] %.1f\n", frames / (now - last));
                frames = 0;
                last = now;
            }
        }

        /* Tout est figé en pause comme pendant un panneau de confirmation : la
         * physique, mais aussi la démo, la caméra d'orbite et la vibration cockpit
         * (qui suivent m_animTime, lequel n'avance pas tant qu'on est figé). */
        const bool frozen = m_paused || m_confirmReset || m_confirmDemo || m_combat.gameOver();
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
            controls = computeControls(rawInput, frameDt, t);
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
           coupées en mode assisté, en démo et en atterrissage automatique, pour garder
           un vol facile et un parcours prévisible. Sans quoi l'atterrissage automatique
           traverse en toute fin d'approche l'enveloppe du vortex ring state (descente
           de 3 à 7 m/s à faible vitesse, voir VRS_DESCENT_MIN/MAX) : la perte de
           portance qui en résulte (jusqu'à -35 %, VRS_THRUST_LOSS) empêche le collectif
           de tenir le taux de descente visé, d'où une arrivée bien plus dure que prévu. */
        m_flight.setRealFlyPhysicsEnabled(!m_assist.active() && !m_demo.active() &&
                                          !m_autoland.active());

        /* État physique avant le dernier pas, conservé pour interpoler le rendu. */
        physics::RigidBody prevBody = m_flight.body();
        if (frozen) {           /* pause ou panneau de confirmation : le vol est figé */
            accumulator = 0.0f; /* pas de rattrapage à la reprise */
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
        const float alpha = accumulator / SIM_DT;
        const vec3 renderPos = glm::mix(prevBody.position, body.position, alpha);
        const quat renderOri = glm::slerp(prevBody.orientation, body.orientation, alpha);

        /* Transformation monde de l'appareil : translation + orientation (quaternion). */
        const mat4 base = glm::translate(mat4(1.0f), renderPos) * glm::mat4_cast(renderOri);

        /* Cap (lacet) extrait de l'orientation, utile pour la caméra de poursuite. */
        const vec3 forward = renderOri * vec3{1.0f, 0.0f, 0.0f};
        const float yaw = std::atan2(-forward.z, forward.x);

        updateCamera(base, renderPos, yaw, t, frameDt);

        const float airspeed = glm::length(vec2{body.velocity.x, body.velocity.z});
        const float turbineFraction = m_flight.turbine().turbineFraction();
        const float rotorFraction = m_flight.turbine().rotorFraction();

        /* Message radio : armé à la turbine au plein régime, émis 2 s après.
           Figé en pause, comme le reste. */
        if (!frozen) {
            updateRadioMessage(turbineFraction, frameDt);
            /* Mode zombie : tir (R3/Ctrl gauche) et avancement de la horde.
               Figé en pause comme le reste, pour ne pas laisser les zombies
               continuer d'agir ni la gâchette tirer pendant un panneau de
               confirmation. */
            m_combat.update(frameDt, body, m_input->fireHeld(), [this](float x, float z) {
                return m_terrain->heightAt(x, z);
            });

            /* Contact avec le sol : la vitesse d'arrivée a été relevée par la
               physique, au pas fixe où elle s'est produite (ici, l'appareil est
               déjà posé, vitesse annulée). On la consomme à chaque image, même
               hors combat, pour qu'un vieux contact ne vienne pas percer le
               réservoir au lancement de la partie suivante. Le mode zombie dit
               combien de kérosène le choc a fait fuir, la physique le retire du
               réservoir. Après update(), qui vide les événements sonores, et avant
               leur lecture ci-dessous. */
            m_flight.drainFuel(m_combat.applyGroundImpact(m_flight.consumeGroundImpact()));

            /* Sons ponctuels du mode zombie, déclenchés sur les événements de
               cette image (voir CombatMode::SoundEvents) : même principe que
               le son de démarrage turbine (comparaison d'état, updateAudio).
               Chacun est spatial (volume selon la distance de sa position
               réelle à l'hélico, ici body.position) et joue une instance
               indépendante par occurrence -- plusieurs zombies touchés la
               même image sonnent donc ensemble plutôt que de se couper la
               parole. Seul playWaveStart (plus bas) échappe à ce principe. */
            const CombatMode::SoundEvents& combatEvents = m_combat.soundEvents();
            if (combatEvents.fired) {
                m_audio.playGunfire(combatEvents.muzzlePos, body.position);
            }
            /* explosionPositions porte une entrée par explosion de roquette,
               kill ou non (voir RocketSystem::UpdateResult) : le bruit
               d'explosion joue à chaque impact, indépendamment du cri de
               zombie touché/tué ci-dessous (habillage optionnel, silencieux
               tant que ses fichiers propres ne sont pas fournis). */
            for (const vec3& soundPos : combatEvents.explosionPositions) {
                m_audio.playExplosion(soundPos, body.position);
            }
            for (const vec3& soundPos : combatEvents.zombieDeathPositions) {
                m_audio.playZombieDeath(soundPos, body.position);
            }
            for (const vec3& soundPos : combatEvents.zombieHitPositions) {
                m_audio.playZombieHit(soundPos, body.position);
            }
            for (const vec3& soundPos : combatEvents.throwPositions) {
                m_audio.playToxicThrow(soundPos, body.position);
            }
            if (combatEvents.impacted) {
                m_audio.playToxicImpact(body.position, body.position);
            }
            if (combatEvents.waveStart) {
                m_audio.playWaveStart();
            }
            if (combatEvents.broodSpawned) {
                m_audio.playBroodSpawn();
            }
        }

        updateAudio(body, controls, airspeed, turbineFraction, rotorFraction, frameDt);
        advanceRotor(rotorFraction, frameDt);

        /* Souffle rotor : poussière soulevée au ras du sol. Pas de temps nul en
           pause, ce qui fige le nuage en place au lieu de le faire disparaître. */
        updateSouffle(base, rotorFraction, controls.collective, frozen ? 0.0f : frameDt);

        /* Tuiles fines : la fenêtre de détail suit la caméra, et non l'appareil,
           parce que c'est elle qui décide de ce qui est à l'écran (vue orbite
           lointaine comprise). Hors du test de pause, à dessein : le
           chargement des tuiles et leur apparition doivent se terminer même si
           la simulation est figée, sinon le décor resterait flou sous les yeux
           du joueur. */
        m_terrain->suivreDetail(m_camera.position().x, m_camera.position().z, frameDt);

        renderScene(base,
                    m_rotorAngle,
                    rotorFraction,
                    controls.pedals,
                    controls.cyclicLongitudinal,
                    controls.cyclicLateral,
                    controls.collective,
                    turbineFraction,
                    t);

        ui::HudData hud;
        fillHud(hud, body, forward, controls, airspeed, turbineFraction, rotorFraction, t, frameDt);
        buildNavHud(hud, body.position, hud.headingDeg, t);
        /* Sous-titre du message radio simulé, tant que son temps d'affichage court. */
        hud.radioMessage = (m_radioMsgShow > 0.0f) ? m_radioMsg.c_str() : "";
        /* Message de l'atterrissage automatique (échec de l'engagement ou
           auto-désengagement), tant que son temps d'affichage court. */
        if (m_autolandMsgShow > 0.0f) {
            m_autolandMsgShow -= frameDt;
        }
        hud.autolandMessage = (m_autolandMsgShow > 0.0f) ? m_autolandMsg.c_str() : "";
        /* En démo, le HUD est éteint mais on garde les étiquettes des lieux. */
        m_hud.updateScale(m_width, m_height); /* échelle et police, avant le NewFrame ImGui */
        m_hud.render(hud, m_hudMode, m_paused, m_confirmReset, m_confirmDemo, m_demo.active());

        glfwSwapBuffers(m_window);
    }

    return m_returnToMenu; /* true = retour au menu ; false = fenêtre fermée (on quitte) */
}

} /* namespace artouste::app */
