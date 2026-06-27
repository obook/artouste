/*
 * ApplicationHud.cpp
 * Construction des données du HUD : d'une part les instruments (altitude, vitesse,
 * cap, régimes, carburant...) lus dans l'état physique, d'autre part le repérage
 * (minimap et étiquettes des lieux remarquables projetées sur la scène).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "render/Camera.hpp"
#include "render/LoadedHelicopter.hpp"
#include "render/Terrain.hpp"
#include "ui/Hud.hpp"
#include "util/Math.hpp"

#include <cmath>

namespace artouste::app {

namespace {

/* Aide à l'atterrissage (mode assisté). Seuils calés sur l'Alouette II SE 3130. */

/* Distance horizontale maximale pour chercher un pad autour de l'appareil (m). */
constexpr float PAD_SEARCH_RADIUS_M = 300.0f;

/* Conditions d'activation du réticule : finale, basse vitesse. */
constexpr float PAD_GUIDE_MAX_ALT_M = 50.0f;   /* altitude max au-dessus du pad */
constexpr float PAD_GUIDE_MIN_ALT_M = -2.0f;   /* en dessous : on est sous le sol du pad */
constexpr float PAD_GUIDE_MAX_KT    = 20.0f;   /* vitesse air max */

/* Conditions de détection du posé. */
constexpr float PAD_LAND_MAX_ALT_M  = 0.8f;    /* hauteur patins (~0,5 m) + marge */
constexpr float PAD_LAND_MAX_SPEED  = 2.0f;    /* vitesse sol max (m/s) pour valider */

/* Durée d'affichage du score après le posé (s). */
constexpr float SCORE_DISPLAY_S     = 5.0f;

}  /* namespace anonyme */

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
    const char* nom     = nullptr;
    float       distMin = PAD_SEARCH_RADIUS_M * PAD_SEARCH_RADIUS_M;

    /* Hélipads propres au terrain. */
    if (m_terrain) {
        for (const render::Landmark& pad : m_terrain->helipads()) {
            float px = 0.0f, pz = 0.0f;
            m_terrain->worldAt(pad.lon, pad.lat, px, pz);
            const float dx = heliPos.x - px;
            const float dz = heliPos.z - pz;
            const float d2 = dx * dx + dz * dz;
            if (d2 < distMin) {
                distMin   = d2;
                nom       = pad.name.c_str();
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
            distMin   = d2;
            nom       = "Pad depart";
            poseMonde = m_startPos;
        }
    }

    return nom;
}

void Application::fillHud(ui::HudData& hud, const physics::RigidBody& body, const vec3& forward,
                          const physics::Controls& controls, float airspeed, float turbineFraction,
                          float rotorFraction, float t, float frameDt) {
    hud.altitudeM  = body.position.y;
    hud.airspeedKt = airspeed * 1.94384f;
    /* Cap boussole pour le HUD (ruban de cap, texte HDG, flèche de la minimap) :
       0 = nord, 90 = est, sens horaire. Le repère monde a X vers l'est et Z vers
       le sud, donc le nord est -Z. (Le lacet 'yaw' calculé dans la boucle, mesuré
       depuis l'est, reste réservé à la caméra de poursuite.) */
    float headingDeg = glm::degrees(std::atan2(forward.x, -forward.z));
    if (headingDeg < 0.0f) {
        headingDeg += 360.0f;
    }
    hud.headingDeg    = headingDeg;
    hud.varioFpm      = body.velocity.y * 196.85f;
    hud.varioMs       = body.velocity.y;
    hud.collectivePct = controls.collective * 100.0f;
    hud.rotorPct      = rotorFraction * 100.0f;          /* régime rotor, en pourcentage */
    hud.rotorRpm      = rotorFraction * 360.0f;          /* régime rotor nominal : 360 tr/min */
    hud.turbineRpm    = turbineFraction * 33500.0f;      /* régime turbine nominal : ~33 500 tr/min */
    hud.exhaustTempC  = m_flight.turbine().exhaustTempC();  /* température tuyère (T4) */
    hud.fuelLiters    = m_flight.fuelLiters();
    hud.turbine       = m_flight.turbine().label();
    hud.assist        = m_assist.active();
    hud.radio         = m_audio.radioPlaying();
    hud.radioMixPct   = static_cast<int>(m_audio.radioMix() * 100.0f + 0.5f);
    if (m_terrain->hasGeo()) {  /* longitude / latitude de l'appareil */
        float lon = 0.0f, lat = 0.0f;
        m_terrain->lonLatAt(body.position.x, body.position.z, lon, lat);
        hud.geoValid = true;
        hud.lonDeg   = lon;
        hud.latDeg   = lat;
    }
    /* Heure du simulateur (réelle ou accélérée) : voir Application::timeOfDaySeconds.
       Le deux-points clignote à 1 Hz sur le temps réel écoulé (allumé une demi-seconde
       sur deux), comme une horloge digitale. */
    hud.timeOfDaySec = timeOfDaySeconds(t);
    hud.timeScale    = m_sunTimeScale;
    hud.colonOn      = (std::fmod(t, 1.0f) < 0.5f);

    /* Aide à l'atterrissage : calcule l'écart au pad le plus proche (réticule de
       centrage) et le score au posé. En démo, c'est la phase qui commande, et elle
       seule : aide uniquement au retour/pose, jamais au décollage, même si le mode
       assisté avait été laissé actif avant de lancer la démo. Hors démo, en pilotage
       manuel, l'aide suit le mode assisté. */
    const bool aideAtterrissage = m_demo.active()
                                ? m_demo.returning()
                                : m_assist.active();
    hud.padGuidance = {};
    if (aideAtterrissage) {
        /* Point de référence : le mât rotor, pas l'origine du modèle. Au parking,
           l'origine est reculée de ROTOR_FORWARD_OFFSET pour centrer le mât sur le H
           (voir ApplicationScene). Mesurer l'écart depuis le mât évite un biais
           longitudinal constant qui décalait le trait horizontal du réticule. */
        const vec3  mat = body.position
                        + forward * render::LoadedHelicopter::ROTOR_FORWARD_OFFSET;
        vec3        posePad{0.0f, 0.0f, 0.0f};
        const char* nomPad = padPlusProche(mat, posePad);
        if (nomPad) {
            ui::HudData::PadGuidance& g = hud.padGuidance;

            /* Ecart en coordonnées monde, puis projeté dans le repère pilote. */
            const float dxMonde   = mat.x - posePad.x;
            const float dzMonde   = mat.z - posePad.z;
            const float dist2D    = std::sqrt(dxMonde * dxMonde + dzMonde * dzMonde);
            const float altSurPad = mat.y - posePad.y;

            /* right : axe latéral pilote (droite positive) dérivé de forward. */
            const vec3 right = glm::normalize(glm::cross(forward, vec3{0.0f, 1.0f, 0.0f}));
            const vec3 ecart{dxMonde, 0.0f, dzMonde};

            g.distanceM   = dist2D;
            g.altAbovePad = altSurPad;
            g.name        = nomPad;
            g.dx          = glm::dot(ecart, right);     /* + = pad à droite du pilote */
            g.dz          = glm::dot(ecart, forward);   /* + = pad devant */

            /* Réticule visible en finale basse vitesse seulement. */
            g.active = (altSurPad < PAD_GUIDE_MAX_ALT_M)
                    && (altSurPad > PAD_GUIDE_MIN_ALT_M)
                    && (hud.airspeedKt < PAD_GUIDE_MAX_KT);

            /* Détection du posé : appareil quasi immobile très près du sol du pad.
               On ne compte un score que si l'appareil a d'abord volé (m_wasAirborne),
               pour ne pas déclencher un faux "PARFAIT" en activant l'aide alors qu'on
               est déjà posé, ou au tout début avant le décollage. */
            if (altSurPad > PAD_LAND_MAX_ALT_M) {
                m_wasAirborne = true;
            }
            const float vitesseSol = glm::length(vec3{body.velocity.x, 0.0f, body.velocity.z});
            const bool  surSol     = (altSurPad < PAD_LAND_MAX_ALT_M)
                                  && (vitesseSol < PAD_LAND_MAX_SPEED);
            if (surSol && !m_wasOnGround && m_wasAirborne) {
                /* Front montant après un vol : enregistrer le score du posé. */
                m_lastScoreM  = dist2D;
                m_scoreTimer  = SCORE_DISPLAY_S;
                m_wasAirborne = false;
            }
            m_wasOnGround = surSol;
        } else {
            m_wasOnGround = false;
            m_wasAirborne = false;
        }

        /* Affichage du score pendant SCORE_DISPLAY_S secondes après le posé. */
        if (m_scoreTimer > 0.0f) {
            hud.padGuidance.scoreM = m_lastScoreM;
            hud.padGuidance.scored = true;
            m_scoreTimer -= frameDt;
            if (m_scoreTimer < 0.0f) {
                m_scoreTimer = 0.0f;
            }
        }
    } else {
        /* Aide inactive (ni assisté, ni démo en retour) : on oublie l'état du posé. */
        m_wasOnGround = false;
        m_wasAirborne = false;
        m_scoreTimer  = 0.0f;
    }
}

void Application::buildNavHud(ui::HudData& hud, const vec3& heliPos, float headingDeg) {
    if (!m_terrain || !m_terrain->hasGeo()) {
        return;
    }
    const float halfW = m_terrain->halfWidth();
    const float halfH = m_terrain->halfHeight();

    /* Minimap : orthophoto + position de l'appareil (fractions 0-1 dans l'emprise). */
    hud.mapTexId      = m_terrain->orthoTexId();
    hud.mapHeliU      = heliPos.x / (2.0f * halfW) + 0.5f;
    hud.mapHeliV      = heliPos.z / (2.0f * halfH) + 0.5f;
    /* headingDeg est déjà un cap boussole (0 = nord, 90 = est, sens horaire), ce
       qu'attend la flèche de la minimap (nord en haut). */
    hud.mapHeadingDeg = headingDeg;

    /* Étiquette 3D + point minimap d'un lieu (nom + position WGS84), s'il tombe dans
       l'emprise du terrain courant. Projetée légèrement au-dessus du sol. */
    const mat4 viewProj = m_camera.proj() * m_camera.view();
    const auto addLabel = [&](const render::Landmark& place, const char* displayName) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(place.lon, place.lat, x, z);
        if (std::fabs(x) > halfW || std::fabs(z) > halfH) {
            return;  /* hors du terrain courant */
        }
        ui::HudLabel label;
        label.name = displayName;
        label.mapU = x / (2.0f * halfW) + 0.5f;
        label.mapV = z / (2.0f * halfH) + 0.5f;

        const float y    = m_terrain->heightAt(x, z) + 25.0f;
        const vec4  clip = viewProj * vec4{x, y, z, 1.0f};
        if (clip.w > 0.1f) {
            const vec3 ndc = vec3(clip) / clip.w;
            if (ndc.z < 1.0f && std::fabs(ndc.x) < 1.02f && std::fabs(ndc.y) < 1.02f) {
                label.fx       = ndc.x * 0.5f + 0.5f;
                label.fy       = 1.0f - (ndc.y * 0.5f + 0.5f);
                label.depth    = clip.w;  /* distance caméra : sert à donner la priorité au plus proche */
                label.onScreen = true;
            }
        }
        hud.labels.push_back(label);
    };

    /* Lieux remarquables (étiquetés par leur nom), puis hélipads (tous étiquetés
       "Hélisurface", le terme d'aire de poser ; leur ville est déjà donnée par le
       lieu remarquable voisin). */
    for (const render::Landmark& lm : m_terrain->landmarks()) {
        addLabel(lm, lm.name.c_str());
    }
    for (const render::Landmark& pad : m_terrain->helipads()) {
        addLabel(pad, "Hélisurface");
    }
}

}  /* namespace artouste::app */
