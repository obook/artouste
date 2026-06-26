/*
 * ApplicationInput.cpp
 * Entrées d'action (hors commandes de vol) : boutons et croix de la manette lus
 * à chaque image, et callback clavier de GLFW. Les commandes de vol elles-mêmes
 * (manche, palonnier, collectif) sont lues par InputSystem dans la boucle.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <GLFW/glfw3.h>

#include "input/InputSystem.hpp"
#include "ui/Hud.hpp"

namespace artouste::app {

void Application::handleActionButtons() {
    /* Pendant la démo, les boutons B (HUD) et Y (vue) agissent comme les touches H et C
     * du clavier, sans couper la démo : l'utilisateur reprend simplement la main sur le
     * HUD ou la vue. Les autres boutons sont ignorés pour ne pas perturber la
     * chorégraphie. Pour sortir de la démo à la manette, il suffit de reprendre les
     * commandes (manche), voir updateControls (pilotInput). */
    if (m_demo.active()) {
        if (m_input->hudTogglePressed()) {  /* B : change de HUD, la démo continue */
            m_hudMode = static_cast<ui::HudMode>((static_cast<int>(m_hudMode) + 1) % 3);
            m_demoUserHud = true;
        }
        if (m_input->viewTogglePressed()) {  /* Y : change de vue, la démo continue */
            m_viewMode     = (m_viewMode + 1) % 4;
            m_demoUserView = true;
        }
        return;
    }

    if (m_input->assistTogglePressed()) {  /* croix haut : mode assisté (touche M) */
        m_assist.toggle();
    }

    /* Bouton Y de la manette : change de vue, comme la touche C du clavier. */
    if (m_input->viewTogglePressed()) {
        m_viewMode = (m_viewMode + 1) % 4;
    }

    /* Bouton Start de la manette : démarre ou coupe la turbine, comme la touche T. */
    if (m_input->turbineTogglePressed()) {
        m_flight.turbine().toggle();
    }

    /* Boutons manette équivalents aux touches clavier H, P, R et Échap, pour
     * pouvoir jouer à la manette seule. */
    if (m_confirmReset) {
        /* Panneau de confirmation affiché : A = Oui, B = Non. Les autres actions
         * des boutons sont neutralisées tant qu'on n'a pas répondu. */
        if (m_input->liveryTogglePressed()) {  /* A : Oui -> reset */
            resetToStart();
        } else if (m_input->hudTogglePressed()) {  /* B : Non -> on annule */
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
    } else {
        if (m_input->hudTogglePressed()) {  /* B : fait défiler les modes HUD (comme H) */
            m_hudMode = static_cast<ui::HudMode>((static_cast<int>(m_hudMode) + 1) % 3);
        }
        if (m_input->pauseTogglePressed()) {  /* Back : pause/reprise (comme P) */
            m_paused = !m_paused;
        }
        if (m_input->resetPressed()) {  /* X : demande la confirmation du reset (comme R) */
            m_confirmReset = true;
        }
        if (m_input->quitPressed()) {  /* LB + RB : quitte (comme Échap) */
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        if (m_input->liveryTogglePressed()) {  /* A : bascule la livrée (comme L) */
            toggleGendarmerieLivery();
        }
    }
}

void Application::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action,
                              int /*mods*/) {
    if (action != GLFW_PRESS) {
        return;
    }
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    /* Pendant la démo : seules quelques touches agissent, sans couper la démo. Échap
       sort de la démo (et seulement elle : on ne quitte pas l'application). K et +/-
       pilotent la radio, H le HUD, C la vue ; pour H et C l'utilisateur reprend la main
       (la démo cesse alors de les imposer). Tout le reste est ignoré pour ne pas
       perturber la chorégraphie. */
    if (app != nullptr && app->m_demo.active()) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                app->m_demo.stop();
                break;
            case GLFW_KEY_K:  /* radio internet on/off */
                app->m_audio.toggleRadio(app->m_radioUrl);
                break;
            case GLFW_KEY_MINUS:
            case GLFW_KEY_KP_SUBTRACT:  /* balance vers l'hélico */
                app->m_audio.adjustRadioMix(-0.05f);
                break;
            case GLFW_KEY_EQUAL:
            case GLFW_KEY_KP_ADD:  /* balance vers la radio */
                app->m_audio.adjustRadioMix(0.05f);
                break;
            case GLFW_KEY_H:  /* cycle des modes HUD : l'utilisateur reprend la main */
                app->m_hudMode =
                    static_cast<ui::HudMode>((static_cast<int>(app->m_hudMode) + 1) % 3);
                app->m_demoUserHud = true;
                break;
            case GLFW_KEY_C:  /* change de vue : l'utilisateur reprend la main */
                app->m_viewMode     = (app->m_viewMode + 1) % 4;
                app->m_demoUserView = true;
                break;
            default:
                break;  /* ignoré : la démo continue */
        }
        return;
    }

    /* Panneau de confirmation du reset affiché : seules les réponses Oui/Non sont
       prises en compte (O ou Entrée = Oui, N = Non), tout le reste est ignoré. */
    if (app != nullptr && app->m_confirmReset) {
        if (key == GLFW_KEY_O || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            app->resetToStart();
        } else if (key == GLFW_KEY_N) {
            app->m_confirmReset = false;
        }
        return;
    }

    /* Panneau de confirmation du lancement de la démo : O ou Entrée = Oui (on lance),
       N = Non (on annule), le reste est ignoré. */
    if (app != nullptr && app->m_confirmDemo) {
        if (key == GLFW_KEY_O || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            app->m_confirmDemo = false;
            app->startDemo();
        } else if (key == GLFW_KEY_N) {
            app->m_confirmDemo = false;
        }
        return;
    }

    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_C:  /* change de vue (poursuite -> cockpit -> orbite) */
            if (app != nullptr) {
                app->m_viewMode = (app->m_viewMode + 1) % 4;
            }
            break;
        case GLFW_KEY_T:  /* démarre ou coupe la turbine */
            if (app != nullptr) {
                app->m_flight.turbine().toggle();
            }
            break;
        case GLFW_KEY_V:  /* lance ou arrête la démonstration automatique */
            if (app != nullptr) {
                app->toggleDemo();
            }
            break;
        case GLFW_KEY_L:  /* bascule la livrée Gendarmerie nationale */
            if (app != nullptr) {
                app->toggleGendarmerieLivery();
            }
            break;
        case GLFW_KEY_M:          /* bascule le mode assisté (confort de pilotage) */
        case GLFW_KEY_SEMICOLON:  /* position de la touche "M" sur un clavier AZERTY */
            if (app != nullptr) {
                app->m_assist.toggle();
            }
            break;
        case GLFW_KEY_R:  /* demande la confirmation avant de replacer l'appareil au départ */
            if (app != nullptr) {
                app->m_confirmReset = true;
            }
            break;
        case GLFW_KEY_H:  /* fait défiler les modes HUD : coins -> superposé -> rien */
            if (app != nullptr) {
                app->m_hudMode =
                    static_cast<ui::HudMode>((static_cast<int>(app->m_hudMode) + 1) % 3);
            }
            break;
        case GLFW_KEY_P:  /* met en pause ou reprend */
            if (app != nullptr) {
                app->m_paused = !app->m_paused;
            }
            break;
        case GLFW_KEY_K:  /* allume ou coupe le flux radio internet (en vol libre) */
            if (app != nullptr) {
                app->m_audio.toggleRadio(app->m_radioUrl);
            }
            break;
        case GLFW_KEY_MINUS:        /* balance radio/hélico : vers l'hélico (moins de radio) */
        case GLFW_KEY_KP_SUBTRACT:
            if (app != nullptr) {
                app->m_audio.adjustRadioMix(-0.05f);
            }
            break;
        case GLFW_KEY_EQUAL:        /* balance radio/hélico : vers la radio (plus de radio) */
        case GLFW_KEY_KP_ADD:
            if (app != nullptr) {
                app->m_audio.adjustRadioMix(0.05f);
            }
            break;
        default:
            break;
    }
}

}  /* namespace artouste::app */
