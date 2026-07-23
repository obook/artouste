/*
 * ApplicationControls.cpp
 * Calcule les commandes effectives de l'image : pilote automatique en mode
 * démo ou atterrissage automatique, sinon commandes du pilote passées par le
 * mode assisté. Complète ApplicationLoop.cpp (mainLoop, qui appelle
 * computeControls à chaque image) et ApplicationCameraAudio.cpp (caméra et
 * audio).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "input/InputSystem.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"

#include <cmath>

namespace artouste::app {

physics::Controls
Application::computeControls(const physics::Controls& rawInput, float frameDt, float t) {
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
            const float pilotInput = std::fabs(rawInput.cyclicLateral) +
                                     std::fabs(rawInput.cyclicLongitudinal) +
                                     std::fabs(rawInput.pedals) + std::fabs(rawInput.collective);
            if (pilotInput > 0.15f) {
                m_demo.stop();
                if (m_demoFromMenu) {
                    m_returnToMenu = true; /* démo lancée depuis le menu : on y retourne */
                }
            }
        }
    }

    /* Commandes effectives : fournies par le pilote automatique en mode démo,
     * sinon les commandes du pilote passées par le mode assisté (qui les corrige
     * et les adoucit s'il est actif, sinon les laisse intactes). */
    physics::Controls controls;
    if (m_demo.active()) {
        const physics::RigidBody& demoBody = m_flight.body();
        const vec3 demoFwd = demoBody.orientation * vec3{1.0f, 0.0f, 0.0f};
        const float demoHeading = std::atan2(-demoFwd.z, demoFwd.x);
        const float demoGround = m_terrain->heightAt(demoBody.position.x, demoBody.position.z);
        const DemoPilot::Output demoOut = m_demo.update(frameDt,
                                                        demoBody.position,
                                                        demoBody.velocity,
                                                        demoHeading,
                                                        demoGround,
                                                        m_flight.turbine().rotorFraction(),
                                                        sunDirection(t).y);
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
                case 1:
                    m_hudMode = ui::HudMode::Overlay;
                    break; /* complet (Super HUD) */
                case 2:
                    m_hudMode = ui::HudMode::Corners;
                    break; /* quatre coins */
                default:
                    m_hudMode = ui::HudMode::Off;
                    break; /* aucun */
            }
        }
        if (demoOut.finished) {
            startDemo(); /* la démo est terminée : on la rejoue en boucle */
        }
    } else if (m_autoland.active()) {
        /* Une vraie action du pilote reprend la main tout de suite, comme sortir de la
           démo (même seuil). On compare aux commandes brutes au moment de l'engagement
           (baseline), pas au neutre : le collectif est un levier qui garde sa position
           au repos (clavier comme manette, voir Gamepad::poll), donc rarement nul ; et
           rien n'oblige le pilote à avoir le cyclique pile au neutre à l'instant où il
           déclenche l'atterrissage automatique (il peut être en train de piloter). Sans
           cette référence, la commande déjà tenue au moment de l'engagement dépasserait
           aussitôt le seuil et désengagerait l'instant d'après.
           Les palonniers entrent dans la somme comme les autres axes : le déclencheur
           de l'atterrissage automatique est RB (Gamepad::autolandTogglePressed), une
           gâchette d'épaule indépendante du stick droit (palonniers) -- contrairement
           à l'ancien déclencheur (clic du stick droit, R3), l'engagement ou le
           désengagement de l'atterrissage automatique ne dévie plus cet axe. */
        const physics::Controls& base = m_autoland.baseline();
        const float pilotInput = std::fabs(rawInput.cyclicLateral - base.cyclicLateral) +
                                 std::fabs(rawInput.cyclicLongitudinal - base.cyclicLongitudinal) +
                                 std::fabs(rawInput.pedals - base.pedals) +
                                 std::fabs(rawInput.collective - base.collective);
        if (pilotInput > 0.15f) {
            m_autoland.stop();
            controls = m_assist.apply(rawInput, frameDt);
            m_autolandMsg = "Atterrissage automatique interrompu";
            m_autolandMsgShow = 3.0f;
        } else {
            const physics::RigidBody& body = m_flight.body();
            const vec3 fwd = body.orientation * vec3{1.0f, 0.0f, 0.0f};
            const float heading = std::atan2(-fwd.z, fwd.x);
            const float agl =
                body.position.y - m_terrain->heightAt(body.position.x, body.position.z);
            controls = m_autoland.update(
                frameDt, body.position, body.velocity, heading, agl, [this](float x, float z) {
                    return m_terrain->heightAt(x, z);
                });
            if (!m_autoland.active()) {
                /* Vient de rendre la main (posé, effet de sol évacué) : le levier réel
                   (clavier/manette) n'a pas bougé pendant le vol automatique et garde sa
                   position d'avant l'engagement. Sans resynchronisation, le collectif
                   sauterait dessus à l'image suivante (assistance repliée = commandes
                   brutes telles quelles, voir FlightAssist::apply) et l'appareil
                   redécollerait tout seul. Le mode assisté, lui, n'a pas été sollicité
                   pendant le vol automatique (apply() n'est pas appelé dans cette
                   branche) : son état lissé interne est tout aussi figé sur sa valeur
                   d'avant l'engagement, d'où le même risque s'il est actif en parallèle. */
                m_input->syncCollective(controls.collective);
                m_assist.reset();
            }
        }
    } else {
        controls = m_assist.apply(rawInput, frameDt);
    }
    return controls;
}

} /* namespace artouste::app */
