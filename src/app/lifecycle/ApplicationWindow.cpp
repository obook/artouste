/*
 * ApplicationWindow.cpp
 * Gestion de la fenêtre au runtime : redimensionnement, choix du moniteur et
 * bascule plein écran / fenêtré. Extrait de Application.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include <glad/glad.h>
/* glad doit précéder GLFW. */
#include <GLFW/glfw3.h>

#include <cstdlib>

namespace artouste::app {

namespace {

/* Taille de fenêtre de repli quand on quitte le plein écran sans avoir mémorisé
   de taille antérieure (voir setFullscreen). */
constexpr int WINDOW_WIDTH  = 1280;
constexpr int WINDOW_HEIGHT = 720;

}  /* namespace */

void Application::onResize(int width, int height) {
    m_width  = width;
    m_height = height;
    glViewport(0, 0, width, height);
    if (height > 0) {
        m_camera.setAspect(static_cast<float>(width) / static_cast<float>(height));
    }
}

void Application::resizeCallback(GLFWwindow* window, int width, int height) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app != nullptr) {
        app->onResize(width, height);
    }
}

GLFWmonitor* Application::monitorForWindow() const {
    int           count    = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);

    /* Forçage explicite : ARTOUSTE_MONITOR=<index> impose un écran précis (0 = premier).
       Utile si la détection automatique ne convient pas (ex. Wayland). */
    if (const char* e = std::getenv("ARTOUSTE_MONITOR")) {
        const int idx = std::atoi(e);
        if (idx >= 0 && idx < count) {
            return monitors[idx];
        }
    }

    int wx = 0;
    int wy = 0;
    int ww = 0;
    int wh = 0;
    glfwGetWindowPos(m_window, &wx, &wy);
    glfwGetWindowSize(m_window, &ww, &wh);
    const int cx = wx + ww / 2;  /* centre de la fenêtre, en coordonnées écran */
    const int cy = wy + wh / 2;

    for (int i = 0; i < count; ++i) {
        int mx = 0;
        int my = 0;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        if (mode == nullptr) {
            continue;
        }
        if (cx >= mx && cx < mx + mode->width && cy >= my && cy < my + mode->height) {
            return monitors[i];  /* le centre de la fenêtre tombe sur ce moniteur */
        }
    }
    return glfwGetPrimaryMonitor();  /* position non exploitable (ex. Wayland) : repli */
}

void Application::setFullscreen(bool on) {
    if (on == m_fullscreen) {
        return;
    }
    if (on) {
        /* On mémorise la fenêtre courante pour pouvoir y revenir. */
        glfwGetWindowPos(m_window, &m_winX, &m_winY);
        glfwGetWindowSize(m_window, &m_winW, &m_winH);
        GLFWmonitor*       mon  = monitorForWindow();
        const GLFWvidmode* mode = (mon != nullptr) ? glfwGetVideoMode(mon) : nullptr;
        if (mon != nullptr && mode != nullptr) {
            /* Plein écran sur le moniteur principal à sa résolution native (on reprend
               le mode courant du bureau : mêmes dimensions et rafraîchissement). À
               résolution native, aucun changement de mode visible. Cette voie, via le
               moniteur, fonctionne aussi bien sous Windows que sous Wayland (où GLFW ne
               permet pas de positionner soi-même une fenêtre "sans bordure"). */
            glfwSetWindowMonitor(m_window, mon, 0, 0, mode->width, mode->height,
                                 mode->refreshRate);
        }
        m_fullscreen = true;
    } else {
        const int w = (m_winW > 0) ? m_winW : WINDOW_WIDTH;
        const int h = (m_winH > 0) ? m_winH : WINDOW_HEIGHT;
        glfwSetWindowMonitor(m_window, nullptr, m_winX, m_winY, w, h, 0);
        m_fullscreen = false;
    }
}

void Application::toggleFullscreen() {
    setFullscreen(!m_fullscreen);
}

}  /* namespace artouste::app */
