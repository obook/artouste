/*
 * ApplicationRenderEffects.cpp
 * Lueurs du moteur (feu anti-collision, feux de position, distorsion
 * thermique de la tuyère), dessinées en dernier par-dessus la scène puisque
 * translucides. Appelé depuis renderScene (ApplicationRender.cpp).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "app/AppConstants.hpp"
#include "render/Mesh.hpp"
#include "render/Shader.hpp"
#include "util/Math.hpp"

#include <glad/glad.h>

#include <cmath>

namespace artouste::app {

namespace {

/*
 * Repères des effets moteur, dans le même repère corps que COCKPIT_EYE (X avant,
 * Y haut, Z droite, origine au centre de l'appareil). Réglés à l'oeil via le mode
 * capture (ARTOUSTE_SHOT_*).
 *
 * Strombo (feu anti-collision) : petite sphère rouge posée au-dessus de la cabine,
 * qui clignote (allumée une brève fraction de la période) tant que la turbine tourne.
 */
const vec3      BEACON_POS{3.11f, 2.41f, 0.0f};  /* position du beacon du modèle (all-lights.xml), sur le toit */
constexpr float BEACON_RADIUS = 0.07f;           /* rayon de la sphère (m) */
constexpr float BEACON_PERIOD = 1.2f;            /* période du clignotement (s) */
constexpr float BEACON_ON     = 0.18f;           /* fraction de la période où le feu est allumé */

/*
 * Feux de position avant, aux positions du modèle (all-lights.xml d'Émmanuel),
 * exprimées ici en repère corps. Allumés la nuit (voir renderScene). On respecte la
 * convention aéronautique : rouge à bâbord (gauche), vert à tribord (droite). Le feu
 * blanc de queue du modèle (corps {-3.96, 2.13, 0.20}) n'est pas allumé pour l'instant.
 */
const vec3      NAV_LEFT_POS{4.24f, 0.92f, -0.77f};   /* feu de navigation bâbord (rouge) */
const vec3      NAV_RIGHT_POS{4.24f, 0.92f, 0.77f};   /* feu de navigation tribord (vert) */
constexpr float NAV_RADIUS = 0.05f;                   /* rayon du coeur d'un feu (m) */

/*
 * Tuyère : sortie de la turbine, derrière le bloc moteur, au départ de la poutre.
 * La turbine est haute sur le pont moteur (derrière le mât) : la sortie est donc en
 * arrière de l'origine (X négatif) et en hauteur, pas dans le carter (plus bas, plus
 * en avant). On ne dessine pas de flamme : la turbine rejette de l'air très chaud,
 * qui se traduit par une distorsion thermique localisée (léger halo bleuté qui ondule)
 * et un petit coeur tiède sur le métal de la tuyère.
 */
const vec3      NOZZLE_BODY_POS{-0.30f, 2.28f, 0.0f};
constexpr float NOZZLE_RADIUS = 0.24f;

}  /* namespace */

void Application::drawEngineEffects(const mat4& base, float turbineFraction, float timeSeconds) {
    /* Rien tant que la turbine n'est pas lancée. */
    if (turbineFraction <= 0.01f) {
        return;
    }

    /* Position d'une lueur exprimée en repère corps (X avant, Y haut, Z droite),
       comme COCKPIT_EYE, puis ramenée dans le monde par la pose 'base'. */
    const auto bodyToWorld = [&](const vec3& bodyPos) {
        return vec3(base * vec4(bodyPos, 1.0f));
    };

    /* Une petite sphère lumineuse de couleur unie, mélangée par-dessus la scène.
       Rendu relatif à la caméra : on retranche m_renderOrigin de la position monde,
       en accord avec la vue relative posée plus bas. */
    const auto drawGlow = [&](const vec3& worldPos, float radius, const vec4& color) {
        if (color.a <= 0.01f) {
            return;
        }
        m_flatShader->setMat4("u_model", glm::translate(mat4(1.0f), worldPos - m_renderOrigin) *
                                             glm::scale(mat4(1.0f), vec3{radius}));
        m_flatShader->setVec4("u_color", color);
        m_glowSphere->draw();
    };

    m_flatShader->use();
    m_flatShader->setMat4("u_view", m_camera.view() * glm::translate(mat4(1.0f), m_renderOrigin));
    m_flatShader->setMat4("u_proj", m_camera.proj());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);  /* les lueurs ne masquent rien : elles s'ajoutent au rendu */

    /* --- Strombo ------------------------------------------------------------- */
    /* Petite sphère rouge au-dessus de la cabine, qui clignote : allumée seulement au
       début de chaque période, éteinte le reste du temps, tant que la turbine tourne. */
    const float beaconPhase = std::fmod(timeSeconds, BEACON_PERIOD) / BEACON_PERIOD;
    if (beaconPhase < BEACON_ON) {
        drawGlow(bodyToWorld(BEACON_POS), BEACON_RADIUS, vec4{1.0f, 0.08f, 0.08f, 0.95f});
    }

    /* --- Feux de position avant (nuit) --------------------------------------- */
    /* Les deux feux de nez (rouge bâbord, vert tribord) s'allument la nuit, entre 18h
       et 6h dans l'heure du simulateur (du coucher au lever du soleil). De jour ils
       restent éteints. */
    const float hourOfDay = timeOfDaySeconds(timeSeconds) / 3600.0f;
    if (hourOfDay >= 18.0f || hourOfDay < 6.0f) {
        const auto drawLight = [&](const vec3& bodyPos, const vec3& rgb) {
            const vec3 w = bodyToWorld(bodyPos);
            drawGlow(w, NAV_RADIUS, vec4{rgb, 0.95f});
            drawGlow(w, NAV_RADIUS * 2.2f, vec4{rgb, 0.22f});
        };
        drawLight(NAV_LEFT_POS, vec3{1.0f, 0.05f, 0.05f});   /* bâbord : rouge */
        drawLight(NAV_RIGHT_POS, vec3{0.05f, 1.0f, 0.10f});  /* tribord : vert */
    }

    /* --- Tuyère -------------------------------------------------------------- */
    /* Air chaud rejeté par la turbine : pas de flamme, mais une distorsion thermique.
       On la suggère par un léger halo bleuté très translucide qui ondule doucement
       (deux sinus de fréquences différentes pour un scintillement non répétitif), plus
       un petit coeur tiède sur le métal de la tuyère. L'effet croît avec le régime. */
    const float heat    = clamp(turbineFraction, 0.0f, 1.0f);
    const float shimmer = 0.85f + 0.15f * std::sin(timeSeconds * 9.0f) * std::sin(timeSeconds * 5.3f);
    const vec3  nozzleWorld = bodyToWorld(NOZZLE_BODY_POS);

    /* Halo bleuté (air chaud) : large, presque transparent, taille qui ondule. */
    drawGlow(nozzleWorld, NOZZLE_RADIUS * shimmer, vec4{0.55f, 0.75f, 1.0f, 0.10f * heat});
    /* Coeur un peu plus dense, plus resserré, légèrement plus chaud (blanc bleuté). */
    drawGlow(nozzleWorld, NOZZLE_RADIUS * 0.5f * shimmer, vec4{0.80f, 0.88f, 1.0f, 0.14f * heat});

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

}  /* namespace artouste::app */
