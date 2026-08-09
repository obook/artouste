/*
 * FlightModel.cpp
 * Calcul du vol : à chaque pas, on additionne les forces et les couples qui
 * s'exercent sur l'hélicoptère, on les transmet au corps rigide, puis on applique
 * les limites de sécurité et le contact avec le sol.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "physics/FlightModel.hpp"

#include "physics/constants.hpp"

#include <cmath>

namespace artouste::physics {

namespace {

/* Traînée sur un axe : -k * v * |v|. Elle s'oppose au mouvement et croît avec
 * le carré de la vitesse. */
float axisDrag(float v, float k) noexcept {
    return -k * v * std::fabs(v);
}

/* Borne une valeur dans l'intervalle [-limit, +limit]. */
float clampAbs(float v, float limit) noexcept {
    return v > limit ? limit : (v < -limit ? -limit : v);
}

/* Rapproche progressivement "current" de "target" : filtre passe-bas du premier
 * ordre, utilisé ici pour le retard de bascule du plan de pales (voir
 * ROTOR_LAG_TAU). */
float approach(float current, float target, float dt, float tau) noexcept {
    if (tau <= 0.0f) {
        return target;
    }
    const float alpha = 1.0f - std::exp(-dt / tau);
    return current + alpha * (target - current);
}

}  /* namespace */

void FlightModel::update(const Controls& controls, float dt) noexcept {
    const float collective = saturate(controls.collective);

    /* Densité relative de l'air : elle décroît avec l'altitude (atmosphère
     * simplifiée, voir AIR_DENSITY_SCALE). La turbine aspire moins d'air et le
     * rotor produit moins de portance en altitude, ce qui finit par interdire le
     * stationnaire en montagne. En mode assisté, densité pleine (pas de pénalité). */
    const float densiteRelative =
        m_realFlyPhysicsEnabled ? std::exp(-m_body.position.y / AIR_DENSITY_SCALE) : 1.0f;

    /* Turbine : on fait avancer son régime avant tout le reste, car la poussée
     * et l'anti-couple en dépendent. Rotor à l'arrêt -> rotorFraction = 0.
     * La charge thermique transmise est la puissance réellement produite
     * (collectif ramené par la densité), pas la position du levier : en altitude
     * le levier est plus haut pour la même portance, et la tuyère ne doit pas en
     * être doublement pénalisée. Au niveau de la mer, rien ne change. */
    m_turbine.update(dt, collective * std::sqrt(densiteRelative));
    const float rotorFraction = m_turbine.rotorFraction();

    const vec3 worldUp{0.0f, 1.0f, 0.0f};
    const vec3 bodyUpWorld = m_body.orientation * worldUp;  /* axe du rotor exprimé dans le repère monde */

    /* --- Forces (repère monde) ---------------------------------------------- */
    /* Poussée du rotor, dirigée selon l'axe vertical du fuselage. Elle est calée
     * pour équilibrer le poids au collectif COLL_HOVER, et proportionnelle au
     * régime du rotor : tant que la turbine n'est pas lancée, pas de portance. */

    /* Carburant : la turbine consomme dès qu'elle tourne, d'autant plus que le
     * collectif demande de la puissance. À sec, on coupe la turbine (panne). */
    const float turbineFraction = m_turbine.turbineFraction();
    if (m_fuelLiters > 0.0f && turbineFraction > 0.0f) {
        const float burnLph =
            (FUEL_BURN_MIN_LPH + (FUEL_BURN_MAX_LPH - FUEL_BURN_MIN_LPH) * collective) *
            turbineFraction;
        m_fuelLiters -= burnLph * (dt / 3600.0f);
        if (m_fuelLiters < 0.0f) {
            m_fuelLiters = 0.0f;
        }
    }
    /* Panne sèche, vérifiée À CHAQUE PAS et non pas seulement dans la branche de
       consommation ci-dessus : le réservoir peut tomber à zéro autrement qu'en
       brûlant, par exemple quand un choc au sol le fend (voir drainFuel et le mode
       zombie). Le test d'origine, logé dans la branche gardée par "carburant > 0",
       ne voyait jamais ce cas : la turbine tournait indéfiniment à sec.

       Vaut aussi pour un redémarrage tenté réservoir vide : la séquence s'amorce,
       puis s'éteint au pas suivant. Rien ne repart sans carburant. */
    if (m_fuelLiters <= 0.0f && m_turbine.state() != Turbine::State::Arret &&
        m_turbine.state() != Turbine::State::Extinction) {
        m_turbine.toggle();
    }

    const float baseThrust  = (MASS * G / COLL_HOVER) * collective * rotorFraction * densiteRelative;

    /* Effet de sol : près du sol, la poussée est renforcée ; ce gain diminue avec
     * la hauteur au-dessus du relief. */
    const float agl          = m_body.position.y - m_groundHeight;  /* hauteur sol (m) */
    const float height       = agl > 0.0f ? agl : 0.0f;
    const float groundEffect = 1.0f + GE_MAX * (1.0f - clamp(height / GE_HEIGHT, 0.0f, 1.0f));

    /* Effet de translation : la portance augmente avec la vitesse horizontale.
     * Sans vent, la vitesse par rapport à l'air égale la vitesse horizontale au sol.
     * translationalLiftFactor sert aussi de bascule au virage coordonné ci-dessous
     * (torque.y) : nulle au stationnaire, pleine une fois la vitesse établie. */
    const float airspeed                = glm::length(vec2{m_body.velocity.x, m_body.velocity.z});
    const float translationalLiftFactor = glm::smoothstep(ETL_V_LOW, ETL_V_HIGH, airspeed);
    const float translationalGain       = 1.0f + ETL_MAX * translationalLiftFactor;

    /* Vortex ring state : réduction de portance en descente rapide à faible vitesse.
     * Trois conditions se cumulent : on descend assez vite, on n'avance presque pas,
     * et le collectif est à puissance partielle (le pic est vers 40 %). Reprendre de
     * la vitesse fait tomber vrsVitesseSol à zéro et dissipe le phénomène. */
    float vrsReduction = 1.0f;
    m_vrsIntensity     = 0.0f;
    if (m_realFlyPhysicsEnabled) {
        const float tauxDescente  = -m_body.velocity.y;  /* positif quand on descend */
        const float vrsDescente   = glm::smoothstep(VRS_DESCENT_MIN, VRS_DESCENT_MAX, tauxDescente);
        const float vrsVitesseSol = 1.0f - glm::smoothstep(0.0f, VRS_AIRSPEED_EXIT, airspeed);
        const float vrsPuissance  = clamp(1.0f - std::fabs(collective - 0.4f) / 0.4f, 0.0f, 1.0f);
        /* Intensité du phénomène (0 = aucun, 1 = plein), exposée au HUD pour l'alerte
           vortex : c'est le produit des trois facteurs, avant conversion en perte de
           portance. Nulle en mode assisté et en démo (physique réelle coupée). */
        m_vrsIntensity = vrsDescente * vrsVitesseSol * vrsPuissance;
        vrsReduction   = 1.0f - VRS_THRUST_LOSS * m_vrsIntensity;
    }

    m_lastThrust      = baseThrust * groundEffect * translationalGain * vrsReduction;
    const vec3 thrust = bodyUpWorld * m_lastThrust;

    const vec3 gravity{0.0f, -MASS * G, 0.0f};

    /* Traînée : calculée dans le repère de l'appareil (différente selon l'axe),
     * puis ramenée dans le repère monde. */
    const vec3 velocityBody = glm::conjugate(m_body.orientation) * m_body.velocity;
    const vec3 dragBody{axisDrag(velocityBody.x, KDRAG_FWD),
                        axisDrag(velocityBody.y, KDRAG_VERT),
                        axisDrag(velocityBody.z, KDRAG_LAT)};
    const vec3 drag = m_body.orientation * dragBody;

    /* VNE : la vitesse à ne pas dépasser décroît avec l'altitude. Au-delà, une
     * traînée d'onde (croissant comme le carré du dépassement) freine l'appareil
     * dans le plan horizontal et matérialise la limite. */
    const float vne = vneAtAltitudeMs(m_body.position.y);
    vec3 vneBrake{0.0f, 0.0f, 0.0f};
    if (m_realFlyPhysicsEnabled && airspeed > vne) {
        const float depassement = airspeed - vne;
        const vec3  horiz{m_body.velocity.x, 0.0f, m_body.velocity.z};
        vneBrake = -glm::normalize(horiz) * (VNE_DRAG_K * depassement * depassement);
    }

    const vec3 force = thrust + gravity + drag + vneBrake;

    /* --- Couples (repère corps) --------------------------------------------- */
    /* Rappel vers l'horizontale : un couple qui ramène l'axe du rotor vers la
     * verticale du monde, exprimé dans le repère de l'appareil. Il est nul quand
     * l'appareil est à plat et ne touche pas au cap (lacet). */
    const vec3 levelBody = glm::conjugate(m_body.orientation) * glm::cross(bodyUpWorld, worldUp);

    /* Vol latéral ou arrière : dans le repère corps, X est l'axe avant et Z l'axe
     * latéral. Au-delà de SIDEWARD_V_MAX (18 kt), le rotor anticouple sature et
     * l'autorité au palonnier diminue de moitié, comme sur l'appareil réel. */
    const float vitesseLaterale = std::fabs(velocityBody.z);
    const float vitesseArriere  = velocityBody.x < 0.0f ? -velocityBody.x : 0.0f;
    const float vitesseCritique = vitesseLaterale > vitesseArriere ? vitesseLaterale : vitesseArriere;
    const float facteurAnticouple = m_realFlyPhysicsEnabled
        ? 1.0f - 0.5f * glm::smoothstep(SIDEWARD_V_MAX, SIDEWARD_V_MAX * 1.4f, vitesseCritique)
        : 1.0f;

    /* Le plan des pales ne bascule pas instantanément avec le manche (précession
     * gyroscopique) : le cyclique passe par un filtre passe-bas avant de produire
     * du couple. Le palonnier, lui, agit sur le rotor de queue, trop petit et trop
     * rapide pour avoir ce retard. */
    m_cyclicLateralLagged      = approach(m_cyclicLateralLagged, controls.cyclicLateral, dt, ROTOR_LAG_TAU);
    m_cyclicLongitudinalLagged = approach(m_cyclicLongitudinalLagged, controls.cyclicLongitudinal, dt, ROTOR_LAG_TAU);

    /* Régimes de vol : les trois mécanismes ci-dessous se superposent et évoluent
     * chacun continûment avec la vitesse air, sans bascule d'un "mode stationnaire"
     * à un "mode avion". Ils ne valent que physique réelle active (comme le VRS) :
     * en mode assisté et en démo, l'appareil garde sa réponse de base.
     *
     * Physique réelle coupée (mode assisté, démo, atterrissage automatique) :
     * facteurTranslation vaut 1, donc l'appareil garde EXACTEMENT sa tenue
     * d'avant, la plus stable et la plus prévisible, à toute vitesse. Les deux
     * effets transitoires plus bas, eux, sont purement et simplement absents.
     *
     * 1. Raffermissement aérodynamique : à vitesse établie, le rotor brasse de
     *    l'air neuf et le stabilisateur horizontal mord, ce qui pose l'appareil en
     *    tangage et en roulis. On majore donc les amortissements avec le même
     *    coefficient de translation que la poussée. Au stationnaire, facteur nul :
     *    le cyclique se contente d'incliner le disque et de vectoriser la poussée,
     *    régime volontairement plus neutre et plus délicat.
     *
     *    Cette tenue-là suit la vitesse air LONGITUDINALE, et non la vitesse
     *    horizontale qui sert au gain de poussée : le stabilisateur horizontal ne
     *    voit l'écoulement que d'avant en arrière. Une dérive latérale ou un vol
     *    arrière ne raffermissent donc rien, ce qui est bien le régime le plus
     *    instable décrit par la note technique. */
    const float vitesseAvant = velocityBody.x > 0.0f ? velocityBody.x : 0.0f;
    const float facteurTranslation =
        m_realFlyPhysicsEnabled ? glm::smoothstep(ETL_V_LOW, ETL_V_HIGH, vitesseAvant) : 1.0f;
    const float amortissementAero = 1.0f + STAB_AERO_GAIN * facteurTranslation;

    /*    Le rappel artificiel à l'horizontale suit le même chemin, en sens inverse :
     *    sans vitesse air, rien ne s'écoule sur le fuselage ni sur le stabilisateur,
     *    donc rien ne redresse l'appareil. On n'en garde que LEVEL_HOVER_FRAC au
     *    stationnaire, la pleine valeur une fois la vitesse établie. Sans cela ce
     *    rappel, trois fois plus fort que le cyclique, imposait la même assiette
     *    d'équilibre à toute vitesse et rendait les deux régimes indiscernables. */
    const float rappelHorizon =
        LEVEL_GAIN * (LEVEL_HOVER_FRAC + (1.0f - LEVEL_HOVER_FRAC) * facteurTranslation);

    /* 2. Effet de flux transversal : cloche autour de 14 kt, roulis à GAUCHE sur
     *    l'Alouette II (rotor horaire vu de dessus, voir TRANSVERSE_ROLL). */
    const float cloche = glm::smoothstep(TRANSVERSE_V_IN, TRANSVERSE_V_PEAK, airspeed) *
                         (1.0f - glm::smoothstep(TRANSVERSE_V_PEAK, TRANSVERSE_V_OUT, airspeed));
    const float fluxTransversal = m_realFlyPhysicsEnabled ? cloche : 0.0f;

    /* 3. Décrochage de pale reculante : à l'approche de la VNE, cabrage puis roulis
     *    vers la pale reculante, à DROITE ici. Calculé à part de la portance de
     *    translation, pour que le plafond de vitesse ne dépende pas du gain d'ETL
     *    (la traînée d'onde plus haut borne la vitesse, ceci l'annonce au pilote). */
    m_retreatingStall = m_realFlyPhysicsEnabled
        ? glm::smoothstep(RBS_V_ONSET * vne, RBS_V_FULL * vne, airspeed)
        : 0.0f;

    const vec3&     w = m_body.angularVelocity;
    vec3 torque;
    torque.x = m_cyclicLateralLagged * ROLL_CTRL           /* roulis (autour de X) */
               - TRANSVERSE_ROLL * fluxTransversal         /* flux transversal : à gauche */
               + RBS_ROLL * m_retreatingStall              /* pale reculante : à droite */
               + rappelHorizon * levelBody.x - DAMP_ROLL * amortissementAero * w.x;
    /* Lacet (autour de Y). Sur l'Alouette II, le rotor tourne dans le sens horaire
     * vu de dessus : son couple de réaction fait partir le nez vers la gauche, et le
     * pilote compense au palonnier droit. D'où le signe + sur l'anti-couple, qui croît
     * avec le collectif, et le palonnier droit qui ramène le nez vers la droite.
     *
     * Virage coordonné (voir TURN_COORD_GAIN) : le même cyclique latéral qui
     * incline l'appareil (torque.x, signe +) tourne aussi le nez dans le même sens
     * une fois de la vitesse acquise, d'où le signe - (incliner à droite = torque.x
     * positif = virer à droite = torque.y négatif, comme le palonnier droit
     * ci-dessus). translationalLiftFactor coupe ce terme au stationnaire, où
     * l'appareil doit seulement partir en crabe. */
    torque.y = -controls.pedals * YAW_CTRL * facteurAnticouple
               + REACTIVE_TORQUE * (collective - COLL_HOVER) * rotorFraction
               - TURN_COORD_GAIN * m_cyclicLateralLagged * translationalLiftFactor - DAMP_YAW * w.y;
    torque.z = -m_cyclicLongitudinalLagged * PITCH_CTRL    /* tangage (autour de Z) */
               + RBS_PITCH_UP * m_retreatingStall          /* pale reculante : cabrage */
               + rappelHorizon * levelBody.z - DAMP_PITCH * amortissementAero * w.z;

    const vec3 inertia{I_ROLL, I_YAW, I_PITCH};
    m_body.integrate(force, torque, MASS, inertia, dt);

    /* --- Garde-fous numériques ------------------------------------ */
    /* On limite vitesses et rotations pour que la simulation reste stable. */
    m_body.velocity.x = clampAbs(m_body.velocity.x, MAX_SPEED);
    m_body.velocity.y = clampAbs(m_body.velocity.y, MAX_SPEED);
    m_body.velocity.z = clampAbs(m_body.velocity.z, MAX_SPEED);
    m_body.angularVelocity.x = clampAbs(m_body.angularVelocity.x, MAX_OMEGA);
    m_body.angularVelocity.y = clampAbs(m_body.angularVelocity.y, MAX_OMEGA);
    m_body.angularVelocity.z = clampAbs(m_body.angularVelocity.z, MAX_OMEGA);

    /* Contact avec le sol : l'appareil ne descend pas sous le relief. */
    if (m_body.position.y < m_groundHeight) {
        /* Vitesse d'arrivée, relevée AVANT d'annuler la composante verticale --
           après, il n'en reste rien. On prend la vitesse complète et non le seul
           taux de chute : rentrer dans un versant à l'horizontale reste un
           contact avec le sol. Seul le pas qui ENTRE en contact compte, sans quoi
           un appareil posé se blesserait à chaque pas de simulation. */
        if (!m_inGroundContact) {
            m_groundImpactMs = std::max(m_groundImpactMs, glm::length(m_body.velocity));
        }
        m_body.position.y = m_groundHeight;
        if (m_body.velocity.y < 0.0f) {
            m_body.velocity.y = 0.0f;
        }
    }
    m_inGroundContact = m_body.position.y <= m_groundHeight;

    /* Posé sur les patins : tant que la poussée ne dépasse pas le poids, l'appareil
     * reste collé au sol, sans glisser ni tourner. Dès que le collectif suffit à
     * le soulever, il décolle normalement. */
    if (m_body.position.y <= m_groundHeight && m_lastThrust <= MASS * G) {
        m_body.position.y      = m_groundHeight;
        m_body.velocity        = vec3{0.0f, 0.0f, 0.0f};
        m_body.angularVelocity = vec3{0.0f, 0.0f, 0.0f};
    }
}

}  /* namespace artouste::physics */
