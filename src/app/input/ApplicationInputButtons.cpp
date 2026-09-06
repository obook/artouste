/*
 * ApplicationInputButtons.cpp
 * Boutons et croix de la manette (hors commandes de vol) : vue, turbine, HUD,
 * pause, reset, livrée, ainsi que les réponses Oui/Non des panneaux de
 * confirmation. Le callback clavier de GLFW est dans
 * ApplicationInputKeyboard.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "input/InputSystem.hpp"
#include "ui/Hud.hpp"

namespace artouste::app {

void Application::handleActionButtons() {
    /* Croix directionnelle : radio internet, comme les touches K et +/- du clavier.
       Traitée avant tout le reste pour rester active en démo comme devant un panneau
       de confirmation, exactement comme au clavier. Un cran de balance par appui. */
    constexpr float PAS_BALANCE = 0.05f;
    if (m_input->radioTogglePressed()) { /* croix droite : allume ou coupe la radio */
        m_audio.toggleRadio(m_radioUrl);
    }
    if (m_input->radioMixUpPressed()) { /* croix haut : balance vers la radio */
        m_audio.adjustRadioMix(PAS_BALANCE);
    }
    if (m_input->radioMixDownPressed()) { /* croix bas : balance vers l'hélico */
        m_audio.adjustRadioMix(-PAS_BALANCE);
    }

    /* Pendant la démo : B (HUD), Y (vue) et A (livrée) agissent comme les touches H, C
     * et L du clavier, sans couper la démo ; X coupe la démo et replace l'appareil au
     * pad (comme R). Les autres boutons sont ignorés pour ne pas perturber la
     * chorégraphie. On peut aussi sortir de la démo en reprenant simplement les
     * commandes (manche), voir computeControls (pilotInput). */
    if (m_demo.active()) {
        if (m_input->hudTogglePressed()) { /* B : change de HUD, la démo continue */
            m_hudMode = static_cast<ui::HudMode>((static_cast<int>(m_hudMode) + 1) % 3);
            m_etatDemo.hudRepris = true;
        }
        if (m_input->viewTogglePressed()) { /* Y : change de vue, la démo continue */
            m_viewMode = (m_viewMode + 1) % 4;
            m_etatDemo.vueReprise = true;
        }
        if (m_input->liveryTogglePressed()) { /* A : change de livrée, la démo continue */
            cycleLivery();
        }
        if (m_input->resetPressed()) { /* X : quitte la démo (comme R) */
            m_demo.stop();
            if (m_etatDemo.depuisMenu) {
                m_returnToMenu = true; /* la démo venait du menu : on y retourne */
            } else {
                resetToStart(); /* replace l'appareil au pad (le pilote reprend) */
            }
        }
        if (m_input->menuPressed()) { /* LB + RB : quitte la démo et revient au menu */
            m_demo.stop();
            m_returnToMenu = true;
        }
        return;
    }

    if (m_input->assistTogglePressed()) { /* LB (L1) : mode assisté (touche M) */
        toggleAssist();
    }
    if (m_input->autolandTogglePressed()) { /* RB (R1) : atterrissage automatique (touche J) */
        toggleAutoland();
    }

    /* Bouton Y de la manette : change de vue, comme la touche C du clavier. Sans
       effet en mode zombie : la vue reste bloquée en cockpit (voir CombatMode::
       start, qui la force en entrant en combat). */
    if (m_input->viewTogglePressed() && !m_combat.active()) {
        m_viewMode = (m_viewMode + 1) % 4;
    }

    /* Bouton Start de la manette : démarre ou coupe la turbine, comme la touche T. */
    if (m_input->turbineTogglePressed()) {
        m_flight.toggleTurbine(); /* refusé réservoir vide, comme la touche T */
    }

    /* Boutons manette équivalents aux touches clavier H, P, R et Échap, pour
     * pouvoir jouer à la manette seule. */
    if (m_confirmReset) {
        /* Panneau de confirmation affiché : A = Oui, B = Non. Les autres actions
         * des boutons sont neutralisées tant qu'on n'a pas répondu. */
        if (m_input->liveryTogglePressed()) { /* A : Oui -> reset */
            resetToStart();
        } else if (m_input->hudTogglePressed()) { /* B : Non -> on annule */
            m_confirmReset = false;
        }
    } else if (m_confirmDemo) {
        /* Panneau de confirmation de la démo : A = Oui (on lance), B = Non. */
        if (m_input->liveryTogglePressed()) {
            m_confirmDemo = false;
            startDemo();
        } else if (m_input->hudTogglePressed()) {
            m_confirmDemo = false;
        }
    } else if (m_combat.gameOver()) {
        /* Fin de partie du mode zombie (0 PV) : A confirme le retour au menu
           (bandeau et score affichés à l'étape 5 -- ici seule la mécanique
           est en place). Pas de "Non" : la partie est terminée. */
        if (m_input->liveryTogglePressed()) {
            m_combat.stop();
            m_returnToMenu = true;
        }
    } else {
        if (m_input->hudTogglePressed()) { /* B : fait défiler les modes HUD (comme H) */
            m_hudMode = static_cast<ui::HudMode>((static_cast<int>(m_hudMode) + 1) % 3);
        }
        if (m_input->pauseTogglePressed()) { /* Back : pause/reprise (comme P) */
            m_paused = !m_paused;
        }
        if (m_input->resetPressed()) { /* X : demande la confirmation du reset (comme R) */
            demanderRetourAuPad();
        }
        if (m_input->menuPressed()) { /* LB + RB : retour au menu (comme Échap) */
            m_returnToMenu = true;
        }
        if (m_input->liveryTogglePressed()) { /* A : fait défiler la livrée (comme L) */
            cycleLivery();
        }
    }
}

} /* namespace artouste::app */
