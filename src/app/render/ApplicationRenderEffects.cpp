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
#include <vector>

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
    /* Lueurs moteur (turbine coupée = rien à dessiner) et flash de bouche
       (mode zombie, indépendant du régime turbine -- on tire aussi bien à
       plein régime). Sans l'un ni l'autre, rien à faire. */
    const bool engineGlows = turbineFraction > 0.01f;
    const bool muzzleFlash = m_combat.active() && m_combat.muzzleFlashActive();
    /* Roquettes en vol (mode zombie) : peuvent survivre plus longtemps que le
       flash de bouche, donc prises en compte à part dans la condition de
       dessin. Les explosions au sol, elles, sont désormais un modele 3D animé
       dessiné dans la passe principale (voir ApplicationRender.cpp), plus des
       lueurs ici. */
    const std::vector<RocketSystem::RocketView> rockets =
        m_combat.active() ? m_combat.rockets() : std::vector<RocketSystem::RocketView>{};
    /* Explosions au sol : le corps de la boule de feu est le modele 3D animé
       (ApplicationRender.cpp), mais on ajoute ICI un flash lumineux TRES BREF a
       l'impact (début de vie de l'explosion), pour un coup d'éclat immédiat,
       synchrone avec la mort du zombie, sans attendre que l'animation se
       développe. */
    const std::vector<RocketSystem::ExplosionView> blasts =
        m_combat.active() ? m_combat.explosions() : std::vector<RocketSystem::ExplosionView>{};
    if (!engineGlows && !muzzleFlash && rockets.empty() && blasts.empty()) {
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

    /* Traînée lumineuse tendue entre deux points monde : la sphère unité est
       étirée en ellipsoïde le long du segment (axe long = direction du trait),
       pour un traceur continu en un seul dessin plutôt qu'un chapelet de
       sphères. Rendu relatif à la caméra comme drawGlow. */
    const auto drawStreak = [&](const vec3& a, const vec3& b, float radius, const vec4& color) {
        if (color.a <= 0.01f) {
            return;
        }
        const vec3  seg    = b - a;
        const float length = glm::length(seg);
        if (length < 1e-4f) {
            return;
        }
        const vec3 xAxis = seg / length;
        /* Base orthonormée dont X suit le segment ; l'up de référence bascule
           près de la verticale pour éviter un produit vectoriel dégénéré. */
        const vec3 ref   = std::abs(xAxis.y) < 0.99f ? vec3{0.0f, 1.0f, 0.0f} : vec3{1.0f, 0.0f, 0.0f};
        const vec3 zAxis = glm::normalize(glm::cross(xAxis, ref));
        const vec3 yAxis = glm::cross(zAxis, xAxis);
        mat4       rot(1.0f);
        rot[0] = vec4{xAxis, 0.0f};
        rot[1] = vec4{yAxis, 0.0f};
        rot[2] = vec4{zAxis, 0.0f};
        const vec3 mid = 0.5f * (a + b) - m_renderOrigin;
        m_flatShader->setMat4("u_model", glm::translate(mat4(1.0f), mid) * rot *
                                             glm::scale(mat4(1.0f), vec3{length * 0.5f, radius, radius}));
        m_flatShader->setVec4("u_color", color);
        m_glowSphere->draw();
    };

    m_flatShader->use();
    m_flatShader->setMat4("u_view", m_camera.view() * glm::translate(mat4(1.0f), m_renderOrigin));
    m_flatShader->setMat4("u_proj", m_camera.proj());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);  /* les lueurs ne masquent rien : elles s'ajoutent au rendu */

    if (engineGlows) {
        /* --- Strombo ----------------------------------------------------------- */
        /* Petite sphère rouge au-dessus de la cabine, qui clignote : allumée
           seulement au début de chaque période, éteinte le reste du temps. */
        const float beaconPhase = std::fmod(timeSeconds, BEACON_PERIOD) / BEACON_PERIOD;
        if (beaconPhase < BEACON_ON) {
            drawGlow(bodyToWorld(BEACON_POS), BEACON_RADIUS, vec4{1.0f, 0.08f, 0.08f, 0.95f});
        }

        /* --- Feux de position avant (nuit) -------------------------------------- */
        /* Les deux feux de nez (rouge bâbord, vert tribord) s'allument la nuit,
           entre 18h et 6h dans l'heure du simulateur. De jour ils restent éteints. */
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
        /* Air chaud rejeté par la turbine : pas de flamme, mais une distorsion
           thermique, suggérée par un léger halo bleuté qui ondule doucement (deux
           sinus de fréquences différentes pour un scintillement non répétitif),
           plus un petit coeur tiède sur le métal de la tuyère. */
        const float heat        = clamp(turbineFraction, 0.0f, 1.0f);
        const float shimmer     = 0.85f + 0.15f * std::sin(timeSeconds * 9.0f) * std::sin(timeSeconds * 5.3f);
        const vec3  nozzleWorld = bodyToWorld(NOZZLE_BODY_POS);
        drawGlow(nozzleWorld, NOZZLE_RADIUS * shimmer, vec4{0.55f, 0.75f, 1.0f, 0.10f * heat});
        drawGlow(nozzleWorld, NOZZLE_RADIUS * 0.5f * shimmer, vec4{0.80f, 0.88f, 1.0f, 0.14f * heat});
    }

    if (muzzleFlash) {
        /* --- Flash de bouche (mode zombie) --------------------------------------
         * Retour visuel du tir, indépendant du son (les fichiers audio du mode
         * zombie n'existent pas forcément, voir AudioEngineCombat.cpp) : sans lui,
         * un tir qui ne touche rien ne se voit ni ne s'entend, ce qui donne
         * l'impression que la gâchette ne fait rien. Posé au canon visible que
         * CombatMode place déjà devant l'appareil (au-delà de l'oeil du pilote,
         * COCKPIT_EYE.x ~3,55 m), origine commune avec les traceurs. */
        const vec3 flashPos = m_combat.muzzleVisualPos();
        drawGlow(flashPos, 0.30f, vec4{1.0f, 0.95f, 0.55f, 0.90f});
        drawGlow(flashPos, 0.55f, vec4{1.0f, 0.85f, 0.30f, 0.35f});
    }

    /* --- Roquettes en vol ------------------------------------------------------
     * La roquette file du canon vers le sol : ogive lumineuse en pointe et
     * traînée de feu étirée derrière elle (coeur clair doublé d'un halo orangé),
     * pour bien la voir voyager avant l'explosion. */
    for (const RocketSystem::RocketView& rk : rockets) {
        drawStreak(rk.tail, rk.head, 0.18f, vec4{1.0f, 0.55f, 0.20f, 0.55f});
        drawStreak(rk.tail, rk.head, 0.09f, vec4{1.0f, 0.90f, 0.55f, 0.85f});
        drawGlow(rk.head, 0.22f, vec4{1.0f, 0.95f, 0.75f, 0.95f});
    }

    /* --- Flash d'impact des roquettes ------------------------------------------
     * Coup d'éclat lumineux tres bref au tout début de chaque explosion (les
     * ~25 premiers % de sa vie), pour un impact franc et immédiat, synchrone
     * avec la mort du zombie -- le corps 3D de la boule de feu (passe principale)
     * prend le relais. Blanc-jaune qui enfle un peu et s'éteint vite. */
    for (const RocketSystem::ExplosionView& bl : blasts) {
        const float k = 1.0f - clamp(bl.progress / 0.25f, 0.0f, 1.0f);
        if (k <= 0.01f) {
            continue;
        }
        const vec3  c = bl.center + vec3{0.0f, 1.2f, 0.0f};
        const float r = 1.6f + 2.2f * (1.0f - k);  /* enfle en s'éteignant */
        drawGlow(c, r, vec4{1.0f, 0.95f, 0.75f, 0.85f * k});
        drawGlow(c, r * 0.5f, vec4{1.0f, 1.0f, 0.9f, 0.9f * k});
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

}  /* namespace artouste::app */
