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
#include <cstdio>

namespace artouste::app {

namespace {

/* Aide à l'atterrissage (mode assisté). Seuils calés sur l'Alouette II SE 3130. */

/* Distance horizontale maximale pour chercher un pad autour de l'appareil (m). */
constexpr float PAD_SEARCH_RADIUS_M = 300.0f;

/* Conditions d'activation du réticule : finale, basse vitesse. */
constexpr float PAD_GUIDE_MAX_ALT_M = 50.0f;   /* altitude max au-dessus du pad */
constexpr float PAD_GUIDE_MIN_ALT_M = -2.0f;   /* en dessous : on est sous le sol du pad */
constexpr float PAD_GUIDE_MAX_KMH   = 37.0f;   /* vitesse air max (37 km/h ~ 20 kt) */
constexpr float PAD_GUIDE_GRACE_S   = 15.0f;   /* s sans réticule après un décollage du pad */

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
    hud.airspeedKmh = airspeed * 3.6f;  /* m/s -> km/h (unité d'époque de l'Alouette II FR) */
    /* Cap boussole pour le HUD (ruban de cap, texte HDG, flèche de la minimap) :
       0 = nord, 90 = est, sens horaire. Le repère monde a X vers l'est et Z vers
       le sud, donc le nord est -Z. (Le lacet 'yaw' calculé dans la boucle, mesuré
       depuis l'est, reste réservé à la caméra de poursuite.) */
    float headingDeg = glm::degrees(std::atan2(forward.x, -forward.z));
    if (headingDeg < 0.0f) {
        headingDeg += 360.0f;
    }
    hud.headingDeg    = headingDeg;
    hud.varioMs       = body.velocity.y;
    hud.collectivePct = controls.collective * 100.0f;
    hud.rotorPct      = rotorFraction * 100.0f;          /* régime rotor, en pourcentage */
    hud.rotorRpm      = rotorFraction * 360.0f;          /* régime rotor nominal : 360 tr/min */
    /* LED du cadran NR : armée quand le rotor atteint la bande nominale (340 tr/min,
       bas de la bande verte du cadran), désarmée dès que la turbine quitte son régime.
       Pendant le démarrage et l'extinction, le NR est légitimement bas : LED éteinte
       plutôt que rouge en permanence sur le pad. */
    if (m_flight.turbine().state() != physics::Turbine::State::Regime) {
        m_nrLedArmed = false;
    } else if (hud.rotorRpm >= 340.0f) {
        m_nrLedArmed = true;
    }
    hud.rotorLedArmed = m_nrLedArmed;
    hud.turbineRpm    = turbineFraction * 33500.0f;      /* régime turbine nominal : ~33 500 tr/min */
    hud.exhaustTempC  = m_flight.turbine().exhaustTempC();  /* température tuyère (T4) */
    hud.fuelLiters    = m_flight.fuelLiters();
    hud.turbine       = m_flight.turbine().label();
    hud.assist        = m_assist.active();
    hud.vrsIntensity  = m_flight.vrsIntensity();  /* alerte vortex (bandeau HUD) */

    /* Alerte taux de chute (facon GPWS) : descente rapide pres du sol. Enveloppe
       progressive : plus on est bas, plus le taux de chute toléré est faible. Au-dessus
       de GPWS_MAX_AGL, aucune alerte (descente rapide normale en altitude). Coupée en
       démo (le posé automatique la déclencherait). */
    {
        constexpr float GPWS_MIN_AGL   = 2.0f;    /* m : en vol seulement (pas sur le pad) */
        constexpr float GPWS_MAX_AGL   = 120.0f;  /* m : plafond de surveillance */
        constexpr float GPWS_SINK_NEAR = 2.5f;    /* m/s : taux toléré pres du sol */
        constexpr float GPWS_SINK_FAR  = 8.0f;    /* m/s : taux toléré au plafond */
        const float aglM = body.position.y
                         - m_terrain->heightAt(body.position.x, body.position.z);
        const float sink = -hud.varioMs;  /* positif en descente */
        bool        alert = false;
        if (!m_demo.active() && aglM > GPWS_MIN_AGL && aglM < GPWS_MAX_AGL) {
            const float f         = aglM / GPWS_MAX_AGL;  /* 0 au sol -> 1 au plafond */
            const float sinkLimit = GPWS_SINK_NEAR + (GPWS_SINK_FAR - GPWS_SINK_NEAR) * f;
            alert                 = sink > sinkLimit;
        }
        hud.sinkRateAlert = alert;
    }

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
    /* Clignotement des LED d'alarme jaune/rouge : cadence rapide (~2 Hz, allumée un peu
       plus de la moitié du temps) pour accrocher l'oeil, distincte du deux-points à 1 Hz. */
    hud.alarmBlinkOn = (std::fmod(t, 0.5f) < 0.3f);

    /* Cadence affichée (images/s) : moyenne mobile exponentielle du frameDt instantané,
       pour un chiffre stable. Le frameDt est borné à 0,1 s dans la boucle (plancher à
       10 Hz), donc la valeur sature vers 10 en cas de forte chute -- justement le seuil
       sous lequel le simulateur passe au ralenti. */
    if (frameDt > 0.0f) {
        const float instantFps = 1.0f / frameDt;
        m_fpsSmoothed = (m_fpsSmoothed <= 0.0f)
                            ? instantFps
                            : m_fpsSmoothed + (instantFps - m_fpsSmoothed) * 0.1f;
    }
    hud.fps = m_fpsSmoothed;

    /* Aide à l'atterrissage : calcule l'écart au pad le plus proche (réticule de
       centrage) et le score au posé. En démo, c'est la phase qui commande, et elle
       seule : aide uniquement au retour/pose, jamais au décollage, même si le mode
       assisté avait été laissé actif avant de lancer la démo. Hors démo, l'aide est
       TOUJOURS disponible, que le mode assisté soit actif ou non : le réticule ne
       s'affiche de toute façon qu'en finale basse vitesse près d'un pad (voir g.active
       plus bas), donc il n'encombre jamais le vol de croisière. Le rendu (Hud::render)
       la dessine par-dessus tous les modes d'affichage (coins et Super HUD). */
    const bool aideAtterrissage = m_demo.active()
                                ? m_demo.returning()
                                : true;
    hud.padGuidance = {};
    if (m_padGuideGrace > 0.0f) {
        m_padGuideGrace -= frameDt;  /* décompte du délai de grâce après un décollage */
    }
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

            /* Détection du posé : appareil quasi immobile très près du sol du pad.
               On ne compte un score que si l'appareil a d'abord volé (m_wasAirborne),
               pour ne pas déclencher un faux "PARFAIT" en activant l'aide alors qu'on
               est déjà posé, ou au tout début avant le décollage. */
            if (altSurPad > PAD_LAND_MAX_ALT_M) {
                m_wasAirborne = true;
                if (!m_hasFlown) {
                    /* Premier décollage depuis le lancement (ou un reset) : délai de
                       grâce sans réticule. Sans lui, la montée initiale, basse et
                       lente, remplit les conditions de finale et l'aide s'affiche
                       dès les premiers mètres. Une seule fois : aux décollages
                       suivants (posé-décollé, rebond, stationnaire au ras du pad),
                       l'aide doit rester disponible immédiatement. */
                    m_hasFlown      = true;
                    m_padGuideGrace = PAD_GUIDE_GRACE_S;
                }
            }
            const float vitesseSol = glm::length(vec3{body.velocity.x, 0.0f, body.velocity.z});
            const bool  surSol     = (altSurPad < PAD_LAND_MAX_ALT_M)
                                  && (vitesseSol < PAD_LAND_MAX_SPEED);

            /* Réticule visible en finale basse vitesse seulement ; jamais tant que
               l'appareil n'a pas décollé au moins une fois (m_hasFlown : pas d'aide
               au lancement ni au reset, quand on est encore garé sur le pad), ni
               pendant le délai de grâce qui suit le premier décollage. */
            g.active = m_hasFlown
                    && (m_padGuideGrace <= 0.0f)
                    && (altSurPad < PAD_GUIDE_MAX_ALT_M)
                    && (altSurPad > PAD_GUIDE_MIN_ALT_M)
                    && (hud.airspeedKmh < PAD_GUIDE_MAX_KMH);

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
    const auto addLabel = [&](const render::Landmark& place, const char* displayName,
                              bool generic) {
        float x = 0.0f, z = 0.0f;
        m_terrain->worldAt(place.lon, place.lat, x, z);
        if (std::fabs(x) > halfW || std::fabs(z) > halfH) {
            return;  /* hors du terrain courant */
        }
        const float altSol = m_terrain->heightAt(x, z);  /* altitude sol du repère (m ASL) */

        /* Tout label (lieu ou hélipad) porte l'altitude sol du repère puis la distance
           3D qui le sépare de l'appareil. Altitude toujours en mètres. Distance en
           kilomètres au loin, mais repassée en mètres sous 1000 m : en approche "820 m"
           guide la pose là où "0.8 km" resterait trop grossier. Recalculé à chaque
           frame, donc la distance suit le vol en direct. */
        const float dx   = heliPos.x - x;
        const float dy   = heliPos.y - altSol;
        const float dz   = heliPos.z - z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        /* Deux lignes : le nom, puis "altitude  distance" en dessous. Le rendu
           (HudOverlay) centre chaque ligne séparément. */
        char buf[96];
        if (dist < 1000.0f) {
            std::snprintf(buf, sizeof buf, "%s\n%.0f m  %.0f m",
                          displayName, static_cast<double>(altSol), static_cast<double>(dist));
        } else {
            std::snprintf(buf, sizeof buf, "%s\n%.0f m  %.1f km",
                          displayName, static_cast<double>(altSol),
                          static_cast<double>(dist) / 1000.0);
        }

        ui::HudLabel label;
        label.name    = buf;
        label.generic = generic;
        label.mapU    = x / (2.0f * halfW) + 0.5f;
        label.mapV    = z / (2.0f * halfH) + 0.5f;

        const float y    = altSol + 25.0f;
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

    /* Lieux remarquables (étiquetés par leur nom), puis hélipads (étiquetés
       "Hélisurface", le terme d'aire de poser ; leur ville est déjà donnée par le
       lieu remarquable voisin). Chaque étiquette porte en plus l'altitude du repère
       et sa distance à l'appareil (voir addLabel). L'étiquette d'hélipad est marquée
       générique : quand un pad coïncide avec un lieu nommé (sommet du pic du Midi
       d'Ossau, hélipads de bigorre...), c'est le nom du lieu qui reste lisible -- et
       il porte désormais lui aussi l'altitude et la distance. */
    for (const render::Landmark& lm : m_terrain->landmarks()) {
        addLabel(lm, lm.name.c_str(), false);
    }
    for (const render::Landmark& pad : m_terrain->helipads()) {
        addLabel(pad, "Hélisurface", true);
    }
}

}  /* namespace artouste::app */
