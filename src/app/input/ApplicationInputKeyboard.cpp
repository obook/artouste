/*
 * ApplicationInputKeyboard.cpp
 * Callback clavier de GLFW : touches de vue, turbine, HUD, pause, reset,
 * livrée, radio, plein écran, ainsi que les panneaux de confirmation et le
 * jeu de touches restreint actif pendant la démo. Les boutons de la manette
 * sont dans ApplicationInputButtons.cpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "ui/Hud.hpp"

#include <GLFW/glfw3.h>

namespace artouste::app {

void Application::keyCallback(
    GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS) {
        return;
    }
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    /* Menu de démarrage affiché : il gère lui-même ses entrées (clavier + manette) et la
       scène peut ne pas être prête. On ignore donc ici toutes les touches de vol. */
    if (app != nullptr && app->m_inMenu) {
        return;
    }

    /* Pendant la démo : seules quelques touches agissent, sans couper la démo. Échap en
       sort ; si la démo a été lancée depuis le menu, on y retourne (sinon on rend la main
       en vol libre). F bascule le plein écran. P met en pause (et reprend) : la démo se
       fige sur place puis repart. K et +/- pilotent la radio, H le HUD, C la vue ; pour
       H et C l'utilisateur reprend la main (la démo cesse alors de les imposer). Tout le
       reste est ignoré pour ne pas perturber la chorégraphie. */
    if (app != nullptr && app->m_demo.active()) {
        switch (key) {
            case GLFW_KEY_ESCAPE: /* quitte la démo : retour au menu si elle en venait */
                app->m_demo.stop();
                if (app->m_demoFromMenu) {
                    app->m_returnToMenu = true;
                }
                break;
            case GLFW_KEY_F: /* bascule plein écran / fenêtré (actif aussi pendant la démo) */
                app->toggleFullscreen();
                break;
            case GLFW_KEY_P: /* pause/reprise sans couper la démo (vol et démo figés) */
                app->m_paused = !app->m_paused;
                break;
            case GLFW_KEY_K: /* radio internet on/off */
                app->m_audio.toggleRadio(app->m_radioUrl);
                break;
            case GLFW_KEY_MINUS:
            case GLFW_KEY_KP_SUBTRACT: /* balance vers l'hélico */
                app->m_audio.adjustRadioMix(-0.05f);
                break;
            case GLFW_KEY_EQUAL:
            case GLFW_KEY_KP_ADD: /* balance vers la radio */
                app->m_audio.adjustRadioMix(0.05f);
                break;
            case GLFW_KEY_H: /* cycle des modes HUD : l'utilisateur reprend la main */
                app->m_hudMode =
                    static_cast<ui::HudMode>((static_cast<int>(app->m_hudMode) + 1) % 3);
                app->m_demoUserHud = true;
                break;
            case GLFW_KEY_C: /* change de vue : l'utilisateur reprend la main */
                app->m_viewMode = (app->m_viewMode + 1) % 4;
                app->m_demoUserView = true;
                break;
            case GLFW_KEY_L: /* défile la livrée : on peut la changer sans couper la démo */
                app->cycleLivery();
                break;
            case GLFW_KEY_R: /* quitte la démo : retour au menu si elle en venait, sinon reprise en
                                vol */
                app->m_demo.stop();
                if (app->m_demoFromMenu) {
                    app->m_returnToMenu = true;
                } else {
                    app->resetToStart(); /* replace l'appareil au pad (le pilote reprend) */
                }
                break;
            default:
                break; /* ignoré : la démo continue */
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

    /* Fin de partie du mode zombie (0 PV) : O ou Entrée confirme le retour au
       menu, pas de "Non" (la partie est terminée). */
    if (app != nullptr && app->m_combat.gameOver()) {
        if (key == GLFW_KEY_O || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
            app->m_combat.stop();
            app->m_returnToMenu = true;
        }
        return;
    }

    switch (key) {
        case GLFW_KEY_ESCAPE: /* retour au menu de démarrage (et non plus quitter) */
            if (app != nullptr) {
                app->m_returnToMenu = true;
            }
            break;
        case GLFW_KEY_C: /* change de vue (poursuite -> cockpit -> orbite) */
            /* Sans effet en mode zombie : vue bloquée en cockpit. */
            if (app != nullptr && !app->m_combat.active()) {
                app->m_viewMode = (app->m_viewMode + 1) % 4;
            }
            break;
        case GLFW_KEY_F: /* bascule plein écran sans bordure / fenêtré */
            if (app != nullptr) {
                app->toggleFullscreen();
            }
            break;
        case GLFW_KEY_T: /* démarre ou coupe la turbine */
            if (app != nullptr) {
                app->m_flight.turbine().toggle();
            }
            break;
        case GLFW_KEY_L: /* fait défiler la livrée (origine, Gendarmerie, armée de terre) */
            if (app != nullptr) {
                app->cycleLivery();
            }
            break;
        case GLFW_KEY_M:         /* bascule le mode assisté (confort de pilotage) */
        case GLFW_KEY_SEMICOLON: /* position de la touche "M" sur un clavier AZERTY */
            if (app != nullptr) {
                app->m_assist.toggle();
            }
            break;
        case GLFW_KEY_J: /* bascule l'atterrissage automatique vers le pad le plus proche */
            if (app != nullptr) {
                app->toggleAutoland();
            }
            break;
        case GLFW_KEY_R: /* demande la confirmation avant de replacer l'appareil au départ */
            if (app != nullptr) {
                app->m_confirmReset = true;
            }
            break;
        case GLFW_KEY_H: /* fait défiler les modes HUD : coins -> superposé -> rien */
            if (app != nullptr) {
                app->m_hudMode =
                    static_cast<ui::HudMode>((static_cast<int>(app->m_hudMode) + 1) % 3);
            }
            break;
        case GLFW_KEY_P: /* met en pause ou reprend */
            if (app != nullptr) {
                app->m_paused = !app->m_paused;
            }
            break;
        case GLFW_KEY_K: /* allume ou coupe le flux radio internet (en vol libre) */
            if (app != nullptr) {
                app->m_audio.toggleRadio(app->m_radioUrl);
            }
            break;
        case GLFW_KEY_MINUS: /* balance radio/hélico : vers l'hélico (moins de radio) */
        case GLFW_KEY_KP_SUBTRACT:
            if (app != nullptr) {
                app->m_audio.adjustRadioMix(-0.05f);
            }
            break;
        case GLFW_KEY_EQUAL: /* balance radio/hélico : vers la radio (plus de radio) */
        case GLFW_KEY_KP_ADD:
            if (app != nullptr) {
                app->m_audio.adjustRadioMix(0.05f);
            }
            break;
        default:
            break;
    }
}

} /* namespace artouste::app */
