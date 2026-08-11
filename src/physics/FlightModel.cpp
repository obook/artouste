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
#include <iterator>

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

    /* Pas collectif réel : le levier est gradué en degrés de pale, comme la
     * machine (voir PAS_MIN_DEG et suivants). Le pas commande aussi la fraction
     * de la puissance turbine que le rotor ABSORBE : butée élastique respectée
     * (14 degrés), la montée retombe sur la planche du manuel ; plein levier
     * (secours, 15 degrés), tout est absorbé. */
    const float pasDeg = PAS_MIN_DEG + (PAS_MAX_DEG - PAS_MIN_DEG) * collective;
    m_pasDeg = pasDeg;
    const float absorption = 1.0f - ABSORPTION_PENTE * (PAS_MAX_DEG - pasDeg);

    /* Densité relative de l'air : elle décroît avec l'altitude (atmosphère
     * simplifiée, voir AIR_DENSITY_SCALE). La turbine aspire moins d'air et le
     * rotor produit moins de portance en altitude, ce qui finit par interdire le
     * stationnaire en montagne. En mode assisté, densité pleine (pas de pénalité). */
    const float densiteRelative =
        m_realFlyPhysicsEnabled ? std::exp(-m_body.position.y / AIR_DENSITY_SCALE) : 1.0f;
    /* Atmosphère RÉELLE, celle du bilan de puissance et de la thermique turbine
     * (voir AIR_DENSITY_SCALE_REELLE : les deux densités coexistent volontairement). */
    const float densiteReelle = std::exp(-m_body.position.y / AIR_DENSITY_SCALE_REELLE);

    /* Régime transitoire (décollage) : au-dessus de ~3000 m, le plancher
     * POWER_FLAT_W dépasse la puissance continue et la turbine chauffe d'autant
     * plus qu'on s'en sert (pas haut). La surchauffe est un état LENT
     * (SURCHAUFFE_TAU_S) : sous le plafond elle se stabilise en dessous de 1 et
     * le régime se tient indéfiniment, la TMP poussée vers 550 degrés servant
     * d'alarme au pilote ; au-dessus du plafond seulement elle dépasse 1, fait
     * fondre le plancher et force la redescente. Elle retombe d'elle-même une
     * fois le pas réduit ou l'altitude rendue.
     *
     * Sans objet en physique assistée (démo, atterrissage automatique), où le
     * bilan de puissance est débranché : la cible tombe à zéro et l'état revient
     * de lui-même, sinon la TMP passait à l'alarme en altitude sans qu'aucune
     * limite ne s'applique. */
    const float pContinuW     = POWER_ROTOR_W * densiteReelle;
    const float usagePlancher = pContinuW < POWER_FLAT_W
        ? (1.0f - pContinuW / POWER_FLAT_W) / SURCHAUFFE_PLEINE
        : 0.0f;
    const float demandePas = clamp((pasDeg - SURCHAUFFE_PAS_SEUIL_DEG) / SURCHAUFFE_PAS_PLAGE_DEG,
                                   0.0f, 1.0f);
    const float cibleSurchauffe = m_realFlyPhysicsEnabled ? usagePlancher * demandePas : 0.0f;
    m_surchauffe = approach(m_surchauffe, cibleSurchauffe, dt, SURCHAUFFE_TAU_S);
    const float fontePlancher = 1.0f - clamp((m_surchauffe - 1.0f) / FONTE_PLAGE, 0.0f, 1.0f);

    /* Turbine : on fait avancer son régime avant tout le reste, car la poussée
     * et l'anti-couple en dépendent. Rotor à l'arrêt -> rotorFraction = 0.
     * La cible de température suit la loi pas/température du manuel (voir
     * T4_LOI_*), altitude ISA comprise : elle mesure la puissance produite, pas
     * la position du levier. Le transitoire la pousse ensuite vers 550 degrés. */
    const float t4Loi = clamp(T4_LOI_BASE_C + T4_LOI_PAS_C * (pasDeg - T4_LOI_PAS_REF_DEG)
                                  - T4_LOI_ALT_C_PAR_KM * m_body.position.y / 1000.0f,
                              EXHAUST_TEMP_IDLE_C, T4_LOI_PLAFOND_C);
    const float t4Cible = t4Loi + (EXHAUST_TEMP_MAX_C - t4Loi) * clamp(m_surchauffe, 0.0f, 1.0f);
    m_turbine.update(dt, t4Cible);
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

    /* Zone à éviter du diagramme hauteur-vitesse (manuel, planche 1.8, numérisée
     * le 11/08/2026 ; borne prudente valable à 1600 kg, la zone réelle à 1100 kg
     * est plus petite) : couples hauteur-vitesse d'où une autorotation réussie
     * n'est pas garantie en cas de panne. Purement indicatif (bandeau HUD) : on
     * lisse l'entrée et la sortie (~2 s) pour un affichage stable, aucune action
     * sur la physique. Sans objet turbine coupée (on est déjà en autorotation),
     * au sol, ou en physique assistée. */
    if (m_realFlyPhysicsEnabled && m_turbine.state() == Turbine::State::Regime && agl > HV_AGL_MIN_M) {
        const float vKmh = airspeed * 3.6f;
        /* Limites numérisées (km/h, m), interpolation linéaire par morceaux. */
        static constexpr float HAUTE[][2] = {{0.f,150.f},{30.f,136.f},{40.f,127.f},{50.f,117.f},
                                             {60.f,100.f},{65.f,90.f},{70.f,76.f},{72.f,63.f}};
        static constexpr float BASSE[][2] = {{0.f,2.f},{10.f,3.f},{20.f,5.f},{30.f,8.f},{40.f,13.f},
                                             {50.f,21.f},{60.f,30.f},{65.f,37.f},{70.f,48.f},{72.f,63.f}};
        static constexpr float RAPIDE[][2] = {{52.f,0.f},{60.f,5.f},{70.f,9.f},{80.f,13.f},{90.f,17.f},
                                              {100.f,20.f},{120.f,22.f},{140.f,24.f},{185.f,25.f}};
        /* Table prise par référence : sa taille se déduit, elle ne peut pas se
           désynchroniser si un point est ajouté. */
        const auto interp = [](const auto& tab, float x) noexcept {
            const std::size_t n = std::size(tab);
            if (x <= tab[0][0]) return tab[0][1];
            for (std::size_t i = 1; i < n; ++i) {
                if (x <= tab[i][0]) {
                    const float t = (x - tab[i - 1][0]) / (tab[i][0] - tab[i - 1][0]);
                    return tab[i - 1][1] + t * (tab[i][1] - tab[i - 1][1]);
                }
            }
            return tab[n - 1][1];
        };
        const bool zoneLente  = vKmh < 72.0f && agl > interp(BASSE, vKmh)
                                            && agl < interp(HAUTE, vKmh);
        const bool zoneRapide = vKmh > 52.0f && agl < interp(RAPIDE, vKmh);
        m_hvIntensity = approach(m_hvIntensity, (zoneLente || zoneRapide) ? 1.0f : 0.0f, dt, 2.0f);
    } else {
        m_hvIntensity = approach(m_hvIntensity, 0.0f, dt, 2.0f);
    }

    /* Bilan de puissance : on calcule d'abord le taux de montée que la turbine
     * autorise, puis on pénalise le dépassement. Les trois postes de dépense et la
     * puissance disponible ne suivent pas la densité de la même façon, et c'est
     * précisément ce qui fait apparaître le plafond :
     *   - la puissance INDUITE monte quand l'air se raréfie (il faut souffler plus
     *     vite pour porter autant), d'où la division par la racine de la densité ;
     *     elle s'effondre en revanche dès qu'on avance ;
     *   - la puissance de PROFIL suit la densité, comme la traînée des pales ;
     *   - la puissance PARASITE est celle de la traînée avant déjà appliquée à la
     *     cellule, donc traînée x vitesse, avec le même coefficient ;
     *   - la puissance DISPONIBLE suit la densité (la turbine aspire moins d'air).
     * Le solde, divisé par le poids, est le taux de montée disponible. Quand il
     * devient négatif, l'appareil ne peut même plus tenir l'altitude : c'est ce qui
     * borne la vitesse en palier, sans avoir à l'écrire nulle part.
     *
     * La densité utilisée ici est celle de l'atmosphère RÉELLE, et non la densité
     * durcie qui pénalise la sustentation : ce bilan se compare à des performances
     * publiées, il doit donc raisonner sur l'air tel qu'il est. Voir
     * AIR_DENSITY_SCALE_REELLE, qui explique pourquoi les deux coexistent.
     *
     * La pénalité ne porte QUE sur le dépassement : en piquée, la pesanteur fournit
     * l'énergie et la turbine n'est pour rien dans la vitesse acquise, la VNE reste
     * donc atteignable en poussant sur le manche. Neutre quand la physique réelle
     * est coupée (mode assisté, démo, atterrissage automatique), comme le VRS. */
    float penaliteMontee = 0.0f;
    if (m_realFlyPhysicsEnabled) {
        const float chuteInduite  = V_INDUITE_HOVER /
            std::sqrt(airspeed * airspeed + V_INDUITE_HOVER * V_INDUITE_HOVER);
        const float pInduite  = POWER_INDUITE_W * chuteInduite / std::sqrt(densiteReelle);
        /* La puissance de profil monte avec la vitesse : la pale avançante travaille
         * dans un vent relatif de plus en plus fort. La forme classique est en
         * 1 + 4,65 fois le carré du rapport d'avance, ce que V_PROFIL_REF résume. */
        const float monteeProfil = 1.0f + (airspeed / V_PROFIL_REF) * (airspeed / V_PROFIL_REF);
        const float pProfil   = POWER_PROFIL_W * densiteReelle * monteeProfil;
        const float pParasite = KDRAG_FWD * airspeed * airspeed * airspeed;
        /* Puissance offerte : la continue suit la densité, le plancher transitoire
         * la relaie en altitude tant que la surchauffe le permet ; le rotor n'en
         * absorbe que la fraction que le pas autorise (voir ABSORPTION_PENTE). */
        const float pDispo = std::max(pContinuW, POWER_FLAT_W * fontePlancher) * absorption;

        const float monteeDispo = (pDispo - pInduite - pProfil - pParasite) / (MASS * G);
        if (m_body.velocity.y > monteeDispo) {
            penaliteMontee = POWER_CLIMB_K * (m_body.velocity.y - monteeDispo);
        }
    }

    m_lastThrust      = baseThrust * groundEffect * translationalGain * vrsReduction
                      - penaliteMontee;
    if (m_lastThrust < 0.0f) {
        m_lastThrust = 0.0f;
    }
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
