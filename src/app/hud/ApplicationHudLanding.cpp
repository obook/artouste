/*
 * ApplicationHudLanding.cpp
 * Aide à l'atterrissage (mode assisté) : réticule de centrage sur le pad le
 * plus proche et score au posé. Complète ApplicationHudInstruments.cpp (le
 * reste des instruments du HUD, qui appelle updateLandingAid depuis fillHud)
 * et ApplicationHudNav.cpp (repérage : minimap et étiquettes).
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app {

namespace {

/* Aide à l'atterrissage (mode assisté). Seuils calés sur l'Alouette II SE 3130. */

/* Distance horizontale maximale pour chercher un pad autour de l'appareil (m). */
constexpr float PAD_SEARCH_RADIUS_M = 999.0f;

/* Conditions d'activation du réticule : finale, basse vitesse. */
constexpr float PAD_GUIDE_MAX_ALT_M = 50.0f; /* altitude max au-dessus du pad */
constexpr float PAD_GUIDE_MIN_ALT_M = -2.0f; /* en dessous : on est sous le sol du pad */
constexpr float PAD_GUIDE_MAX_KMH = 37.0f;   /* vitesse air max (37 km/h ~ 20 kt) */
constexpr float PAD_GUIDE_GRACE_S = 15.0f;   /* s sans réticule après un décollage du pad */

/* Conditions de détection du posé. altSurPad vient de body.position.y (voir plus
   bas), le point que FlightModel colle exactement à m_groundHeight au contact
   (FlightModel.cpp, "Posé sur les patins") : à la pose réelle, altSurPad vaut donc
   0, pas une hauteur de patin. La marge ci-dessous n'est qu'une tolérance de
   détection (jitter et léger écart de relief entre le pad et la position exacte),
   alignée sur AGL_POSE (DemoPilotDetail.hpp), le seuil auquel l'atterrissage
   automatique coupe lui-même le collectif. Avec 0,8 m, le score s'affichait
   pendant que l'appareil tenait encore un vol stationnaire bas, avant ce contact. */
constexpr float PAD_LAND_MAX_ALT_M = 0.2f; /* tolérance de détection du contact (m) */
constexpr float PAD_LAND_MAX_SPEED = 2.0f; /* vitesse sol max (m/s) pour valider */

/* Durée d'affichage du score après le posé (s). */
constexpr float SCORE_DISPLAY_S = 5.0f;

} // namespace

/*
 * padPlusProche
 * Cherche l'hélipad le plus proche de heliPos parmi les hélipads du terrain
 * (m_terrain->helipads()) et le pad de départ (m_startPos). Remplit poseMonde avec
 * la position monde (avec altitude du relief) du pad retenu et renvoie son nom, ou
 * nullptr si aucun pad n'est dans le rayon PAD_SEARCH_RADIUS_M.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 */
const char* Application::padPlusProche(const vec3& heliPos, vec3& poseMonde) const noexcept {
    const char* nom = nullptr;
    float distMin = PAD_SEARCH_RADIUS_M * PAD_SEARCH_RADIUS_M;

    /* Hélipads propres au terrain. */
    if (m_terrain) {
        for (const render::Landmark& pad : m_terrain->helipads()) {
            float px = 0.0f, pz = 0.0f;
            m_terrain->worldAt(pad.lon, pad.lat, px, pz);
            const float dx = heliPos.x - px;
            const float dz = heliPos.z - pz;
            const float d2 = dx * dx + dz * dz;
            if (d2 < distMin) {
                distMin = d2;
                nom = pad.name.c_str();
                poseMonde = vec3{px, m_terrain->heightAt(px, pz), pz};
            }
        }
    }

    /* Pad de départ (m_startPos) : pas de Landmark associé, nom générique. */
    {
        const float dx = heliPos.x - m_startPos.x;
        const float dz = heliPos.z - m_startPos.z;
        const float d2 = dx * dx + dz * dz;
        if (d2 < distMin) {
            distMin = d2;
            nom = "Pad départ";
            poseMonde = m_startPos;
        }
    }

    return nom;
}

void Application::updateLandingAid(ui::HudData& hud,
                                   const physics::RigidBody& body,
                                   const vec3& forward,
                                   float frameDt) {
    /* Calcule l'écart au pad le plus proche (réticule de centrage) et le score au
       posé. En démo, c'est la phase qui commande, et elle seule : aide uniquement
       au retour/pose, jamais au décollage, même si le mode assisté avait été
       laissé actif avant de lancer la démo. Hors démo, l'aide est TOUJOURS
       disponible, que le mode assisté soit actif ou non : le réticule ne s'affiche
       de toute façon qu'en finale basse vitesse près d'un pad (voir g.active plus
       bas), donc il n'encombre jamais le vol de croisière. Le rendu (Hud::render)
       la dessine par-dessus tous les modes d'affichage (coins et Super HUD).
       Absente en mode zombie : poser l'appareil n'a aucun sens pendant un combat,
       la mire de centrage encombrerait sans raison la mire de tir du canon. */
    const bool aideAtterrissage =
        m_combat.active() ? false : (m_demo.active() ? m_demo.returning() : true);
    hud.padGuidance = {};
    if (m_pose.graceReticuleS > 0.0f) {
        m_pose.graceReticuleS -= frameDt; /* décompte du délai de grâce après un décollage */
    }
    if (aideAtterrissage) {
        /* Point de référence horizontal : le mât rotor, pas l'origine du modèle. Au
           parking, l'origine est reculée de ROTOR_FORWARD_OFFSET pour centrer le mât
           sur le H (voir ApplicationScene). Mesurer l'écart depuis le mât évite un
           biais longitudinal constant qui décalait le trait horizontal du réticule.
           SEULEMENT pour dx/dz : forward suit le tangage de l'appareil (pas seulement
           le cap), donc mat.y varie avec l'assiette (jusqu'à environ
           ROTOR_FORWARD_OFFSET * sin(tangage), plusieurs dizaines de cm à quelques
           degrés de piqué) sans rapport avec la hauteur-sol réelle. altSurPad -- dont
           dépendent le score au posé et la visibilité du réticule -- doit rester basé
           sur body.position.y, le point que la physique de contact utilise réellement
           (voir FlightModel::update, "posé sur les patins") : sinon un appareil encore
           légèrement piqué en finale déclenche le score alors que les patins sont
           encore au-dessus du pad. */
        const vec3 mat = body.position + forward * render::LoadedHelicopter::ROTOR_FORWARD_OFFSET;
        vec3 posePad{0.0f, 0.0f, 0.0f};
        const char* nomPad = padPlusProche(mat, posePad);
        if (nomPad) {
            ui::HudData::PadGuidance& g = hud.padGuidance;

            /* Écart en coordonnées monde, puis projeté dans le repère pilote. */
            const float dxMonde = mat.x - posePad.x;
            const float dzMonde = mat.z - posePad.z;
            const float dist2D = std::sqrt(dxMonde * dxMonde + dzMonde * dzMonde);
            const float altSurPad = body.position.y - posePad.y;

            /* right : axe latéral pilote (droite positive) dérivé de forward. */
            const vec3 right = glm::normalize(glm::cross(forward, vec3{0.0f, 1.0f, 0.0f}));
            const vec3 ecart{dxMonde, 0.0f, dzMonde};

            g.distanceM = dist2D;
            g.altAbovePad = altSurPad;
            g.name = nomPad;
            g.dx = glm::dot(ecart, right);   /* + = pad à droite du pilote */
            g.dz = glm::dot(ecart, forward); /* + = pad devant */

            /* Détection du posé : appareil quasi immobile très près du sol du pad.
               On ne compte un score que si l'appareil a d'abord volé (m_pose.aVole),
               pour ne pas déclencher un faux "PARFAIT" en activant l'aide alors qu'on
               est déjà posé, ou au tout début avant le décollage. */
            if (altSurPad > PAD_LAND_MAX_ALT_M) {
                m_pose.aVole = true;
                if (!m_pose.aDecolle) {
                    /* Premier décollage depuis le lancement (ou un reset) : délai de
                       grâce sans réticule. Sans lui, la montée initiale, basse et
                       lente, remplit les conditions de finale et l'aide s'affiche
                       dès les premiers mètres. Une seule fois : aux décollages
                       suivants (posé-décollé, rebond, stationnaire au ras du pad),
                       l'aide doit rester disponible immédiatement. */
                    m_pose.aDecolle = true;
                    m_pose.graceReticuleS = PAD_GUIDE_GRACE_S;
                }
            }
            const float vitesseSol = glm::length(vec3{body.velocity.x, 0.0f, body.velocity.z});
            const bool surSol =
                (altSurPad < PAD_LAND_MAX_ALT_M) && (vitesseSol < PAD_LAND_MAX_SPEED);

            /* Réticule visible en finale basse vitesse seulement ; jamais tant que
               l'appareil n'a pas décollé au moins une fois (m_pose.aDecolle : pas d'aide
               au lancement ni au reset, quand on est encore garé sur le pad), ni
               pendant le délai de grâce qui suit le premier décollage. */
            g.active = m_pose.aDecolle && (m_pose.graceReticuleS <= 0.0f) &&
                       (altSurPad < PAD_GUIDE_MAX_ALT_M) && (altSurPad > PAD_GUIDE_MIN_ALT_M) &&
                       (hud.airspeedKmh < PAD_GUIDE_MAX_KMH);

            if (surSol && !m_pose.auSolAvant && m_pose.aVole) {
                /* Front montant après un vol : enregistrer le score du posé. */
                m_pose.derniereDistanceM = dist2D;
                m_pose.scoreS = SCORE_DISPLAY_S;
                m_pose.aVole = false;
            }
            m_pose.auSolAvant = surSol;
        } else {
            m_pose.auSolAvant = false;
            m_pose.aVole = false;
        }

        /* Affichage du score pendant SCORE_DISPLAY_S secondes après le posé. */
        if (m_pose.scoreS > 0.0f) {
            hud.padGuidance.scoreM = m_pose.derniereDistanceM;
            hud.padGuidance.scored = true;
            m_pose.scoreS -= frameDt;
            if (m_pose.scoreS < 0.0f) {
                m_pose.scoreS = 0.0f;
            }
        }
    } else {
        /* Aide inactive (ni assisté, ni démo en retour) : on oublie l'état du posé. */
        m_pose.auSolAvant = false;
        m_pose.aVole = false;
        m_pose.scoreS = 0.0f;
    }
}

} /* namespace artouste::app */
