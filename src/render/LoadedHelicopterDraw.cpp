/*
 * LoadedHelicopterDraw.cpp
 * Dessin de l'hélicoptère : orchestration (draw) puis la cellule et ses commandes
 * animées (drawAirframe), les rotors (drawRotors) et la passe transparente des
 * marquages de livrée et des vitrages (drawLivery). Les instruments du tableau de
 * bord sont dessinés par LoadedHelicopterInstruments.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/LoadedHelicopter.hpp"

#include <glad/glad.h>

#include "render/LoadedHelicopterDetail.hpp"
#include "render/Shader.hpp"

#include <cmath>

namespace artouste::render {

using namespace heli_detail;

void LoadedHelicopter::draw(Shader& shader, const mat4& base, float rotorAngle,
                            bool fullPilot, float rudder, float cyclicLong,
                            float cyclicLat, float collective, float rollRad,
                            float pitchRad, float altitudeFt, float varioFpm,
                            float headingRad, float airspeedKt, float torquePct) const {
    /* Correction commune à tout l'appareil : demi-tour autour de l'axe vertical
       (le nez FlightGear est à l'opposé du nôtre) puis remontée pour poser les
       patins au sol. 'root' est la transformation de base de tout l'hélicoptère. */
    const mat4 correction = glm::translate(mat4(1.0f), vec3{0.0f, Y_OFFSET, 0.0f}) *
                            glm::rotate(mat4(1.0f), PI, vec3{0.0f, 1.0f, 0.0f});
    const mat4 root = base * correction;

    /* Passe opaque : cellule et commandes, instruments du tableau de bord, puis
       rotors. Enfin la passe transparente (marquages et vitrages). */
    drawAirframe(shader, root, fullPilot, rudder, cyclicLong, cyclicLat, collective);
    drawInstruments(shader, root, rollRad, pitchRad, altitudeFt, varioFpm, headingRad,
                    airspeedKt, torquePct);
    drawRotors(shader, root, rotorAngle);
    drawLivery(shader, root);
}

void LoadedHelicopter::drawAirframe(Shader& shader, const mat4& root, bool fullPilot,
                                    float rudder, float cyclicLong, float cyclicLat,
                                    float collective) const {
    /* Fuselage (hors vitrages), intérieur et les deux occupants. Les pales du rotor
       principal sont traitées à part, en mélange, pour le fondu selon le régime. */
    drawModel(shader, m_fuselage, root, Pass::Opaque);
    drawModel(shader, m_interior, root, Pass::Opaque);
    /* En vue cockpit, le pilote est dessiné sans jambes (chargées à part) : on
       affiche alors ses jambes animées au palonnier. Le pilote entier (vues
       externes) et le copilote gardent leurs jambes figées. */
    drawModel(shader, fullPilot ? m_pilot : m_pilotCockpit,
              root * glm::translate(mat4(1.0f), PILOT_OFFSET), Pass::Opaque);
    drawModel(shader, m_pilot, root * glm::translate(mat4(1.0f), COPILOT_OFFSET), Pass::Opaque);
    /* Inclinaison du manche cyclique (issue de yoke.xml) : tangage autour de l'axe
       latéral (notre Z), roulis autour de l'axe avant-arrière (notre X), pivot à la
       base du manche. La même rotation servira à faire suivre l'avant-bras droit. */
    const float cyclicPitch = glm::radians(cyclicLong * 15.0f);
    const float cyclicRoll  = glm::radians(cyclicLat * -10.0f);
    const mat4  cyclicR = glm::rotate(mat4(1.0f), cyclicPitch, vec3{0.0f, 0.0f, 1.0f}) *
                          glm::rotate(mat4(1.0f), cyclicRoll, vec3{1.0f, 0.0f, 0.0f});

    if (!fullPilot) {
        /* Jambes du pilote : pivot à la hanche (issu de pilot.xml), rotation autour
           de l'axe latéral (notre Z), opposée d'une jambe à l'autre, ~10 deg par
           unité de palonnier. */
        const vec3 hip       = fgToAssimp(vec3{0.237f, 0.0f, -0.065f});
        const float legAngle = glm::radians(rudder * -10.0f);
        const auto legMat = [&](float angle) {
            return root * glm::translate(mat4(1.0f), PILOT_OFFSET) *
                   glm::translate(mat4(1.0f), hip) *
                   glm::rotate(mat4(1.0f), angle, vec3{0.0f, 0.0f, 1.0f}) *
                   glm::translate(mat4(1.0f), -hip);
        };
        drawModel(shader, m_legLeft, legMat(legAngle), Pass::Opaque);
        drawModel(shader, m_legRight, legMat(-legAngle), Pass::Opaque);

        const mat4 pilotBase  = root * glm::translate(mat4(1.0f), PILOT_OFFSET);
        const mat4 stickXform = glm::translate(mat4(1.0f), CYCLIC_PILOT_OFFSET) * cyclicR *
                                glm::translate(mat4(1.0f), -CYCLIC_PILOT_OFFSET);

        /* Haut du bras : laissé au repos. Il reste rattaché à l'épaule et tient le
           coude en place (donc aucun gonflement). */
        drawModel(shader, m_armUpper, pilotBase, Pass::Opaque);

        /* Poignée : elle suit rigidement le manche (même rotation autour de sa
           base), donc la prise reste parfaite. */
        drawModel(shader, m_grip, root * stickXform * glm::translate(mat4(1.0f), PILOT_OFFSET),
                  Pass::Opaque);

        /* Avant-bras : il pivote autour du coude pour rejoindre le poignet, qui a
           suivi la poignée sur le manche. Le bras reste connecté de l'épaule à la
           poignée (voir forearmTransform). */
        const vec3 elbow  = PILOT_OFFSET + m_elbowLocal;
        const vec3 wrist0 = PILOT_OFFSET + m_wristLocal;
        /* Le poignet (jonction avant-bras/poignée) suit le manche avec la poignée :
           l'avant-bras y amène son extrémité -> raccord exact, sans trou. */
        const vec3 wrist1 = vec3(stickXform * vec4(wrist0, 1.0f));
        drawModel(shader, m_forearm,
                  forearmTransform(pilotBase, m_elbowLocal, elbow, wrist0, wrist1), Pass::Opaque);
    }
    /* Palonniers devant chaque siège : les deux pédales basculent en sens opposé
       (~15 deg par unité de palonnier), autour de l'axe latéral. */
    const float pedalAngle = glm::radians(rudder * -15.0f);
    const auto pedalMat = [&](const vec3& offset, float angle) {
        return root * glm::translate(mat4(1.0f), offset) *
               glm::rotate(mat4(1.0f), angle, vec3{0.0f, 0.0f, 1.0f});
    };
    drawModel(shader, m_pedalLeft, pedalMat(PEDALS_PILOT_OFFSET, pedalAngle), Pass::Opaque);
    drawModel(shader, m_pedalRight, pedalMat(PEDALS_PILOT_OFFSET, -pedalAngle), Pass::Opaque);
    /* Pédales du copilote laissées au neutre : ses jambes, elles, sont figées, des
       pédales animées lui rentreraient dans les pieds. */
    drawModel(shader, m_pedalLeft, pedalMat(PEDALS_COPILOT_OFFSET, 0.0f), Pass::Opaque);
    drawModel(shader, m_pedalRight, pedalMat(PEDALS_COPILOT_OFFSET, 0.0f), Pass::Opaque);
    /* Manches cycliques devant chaque siège. Celui du pilote s'incline avec la
       commande (en vue cockpit, où la main suit) ; celui du copilote reste fixe,
       sa main à lui étant figée. */
    const mat4 pilotCyclic = (!fullPilot)
        ? root * glm::translate(mat4(1.0f), CYCLIC_PILOT_OFFSET) * cyclicR
        : root * glm::translate(mat4(1.0f), CYCLIC_PILOT_OFFSET);
    drawModel(shader, m_cyclic, pilotCyclic, Pass::Opaque);
    drawModel(shader, m_cyclic, root * glm::translate(mat4(1.0f), CYCLIC_COPILOT_OFFSET),
              Pass::Opaque);

    /* Levier de collectif à chaque siège : embase fixe au plancher, levier qui pivote
       autour de sa base. Il est modelé le long de l'axe X (poignée vers l'avant) ; on
       le redresse d'un angle de repos puis on le lève encore avec la commande, le tout
       en tournant autour de l'axe latéral (Z). Un angle négatif relève la poignée. */
    const float leverDeg = -(COLLECTIVE_STAND_DEG + COLLECTIVE_RAISE_DEG * clamp(collective, 0.0f, 1.0f));
    const mat4  leverRot = glm::rotate(mat4(1.0f), glm::radians(leverDeg), vec3{0.0f, 0.0f, 1.0f});
    /* Transformation du levier relative à son embase (grossissement puis pivot). */
    const mat4  leverLocal = glm::scale(mat4(1.0f), vec3{COLLECTIVE_SCALE}) * leverRot;

    struct Seat {
        vec3 pilot;       /* origine du pilote sur ce siège */
        vec3 collective;  /* origine du levier de collectif de ce siège */
    };
    const Seat seats[] = {
        {PILOT_OFFSET, COLLECTIVE_PILOT_OFFSET},
        {COPILOT_OFFSET, COLLECTIVE_COPILOT_OFFSET},
    };
    for (const Seat& s : seats) {
        /* Embase et levier du collectif. */
        const mat4 colAt = glm::translate(mat4(1.0f), s.collective);
        drawModel(shader, m_collectiveBase,
                  root * colAt * glm::scale(mat4(1.0f), vec3{COLLECTIVE_SCALE}), Pass::Opaque);
        drawModel(shader, m_collectiveLever, root * colAt * leverLocal, Pass::Opaque);

        /* Bras gauche : haut du bras laissé au repos (rattaché à l'épaule), avant-bras
           qui pivote au coude pour amener la main sur la poignée du levier. Même
           méthode que le bras droit sur le cyclique (rotation + léger étirement le long
           de l'os pour atteindre exactement la poignée, sans trou). */
        const mat4 pilotBaseL = root * glm::translate(mat4(1.0f), s.pilot);
        drawModel(shader, m_armUpperLeft, pilotBaseL, Pass::Opaque);

        const vec3 gripModel = vec3(colAt * leverLocal * vec4(m_collectiveGripLocal, 1.0f));
        const vec3 elbow = s.pilot + m_elbowLeftLocal;
        const vec3 hand0 = s.pilot + m_handLeftLocal;          /* main au repos */
        drawModel(shader, m_forearmLeft,
                  forearmTransform(pilotBaseL, m_elbowLeftLocal, elbow, hand0, gripModel),
                  Pass::Opaque);
    }
}

void LoadedHelicopter::drawRotors(Shader& shader, const mat4& root, float rotorAngle) const {
    /* L'angle du rotor principal est fourni par l'application (sens horaire vu de
       dessus, comme sur l'Alouette II ; à l'arrêt, une pale est alignée sur l'axe de
       l'appareil). La correction de nez appliquée à 'root' (demi-tour autour de Y)
       place la pale 0 vers l'arrière ; on ajoute donc un demi-tour au rotor pour qu'au
       parking une pale pointe vers l'avant et que les deux autres encadrent la sortie
       turbine. */
    const float mainAngle = rotorAngle + PI;
    const mat4  mainBase  = root * glm::translate(mat4(1.0f), MAIN_HUB) *
                           glm::rotate(mat4(1.0f), mainAngle, vec3{0.0f, 1.0f, 0.0f});
    /* Rotor de queue : son disque est vertical, on bascule donc le rotor de -90
       degrés autour de X avant de le faire tourner autour de son propre axe. Il est
       solidaire du rotor principal, d'où l'angle déduit par le rapport de vitesse
       (et de sens opposé). */
    const float tailAngle = -rotorAngle * (TAIL_SPIN / MAIN_SPIN);
    const mat4  tailBase  = root * glm::translate(mat4(1.0f), TAIL_HUB) *
                           glm::rotate(mat4(1.0f), -HALF_PI, vec3{1.0f, 0.0f, 0.0f}) *
                           glm::rotate(mat4(1.0f), tailAngle, vec3{0.0f, 1.0f, 0.0f});

    /* Position d'une pale du rotor principal, répartie autour de l'axe (une pale
       unique recopiée et tournée), au niveau du plan rotor. */
    const auto mainBladeMat = [&](int k) {
        const float heading =
            static_cast<float>(k) * (TWO_PI / static_cast<float>(MAIN_BLADES));
        return mainBase * glm::rotate(mat4(1.0f), heading, vec3{0.0f, 1.0f, 0.0f}) *
               glm::translate(mat4(1.0f), vec3{0.0f, MAIN_BLADE_RISE, 0.0f});
    };

    drawModel(shader, m_mainHub, mainBase, Pass::Opaque);
    for (int k = 0; k < MAIN_BLADES; ++k) {
        drawModel(shader, m_mainBlade, mainBladeMat(k), Pass::Opaque);
    }
    drawModel(shader, m_tailHub, tailBase, Pass::Opaque);
    for (int k = 0; k < TAIL_BLADES; ++k) {
        const float heading =
            static_cast<float>(k) * (TWO_PI / static_cast<float>(TAIL_BLADES));
        const mat4 bladeMat = tailBase *
                              glm::rotate(mat4(1.0f), heading, vec3{0.0f, 1.0f, 0.0f}) *
                              glm::translate(mat4(1.0f), vec3{0.0f, TAIL_BLADE_RISE, 0.0f});
        drawModel(shader, m_tailBlade, bladeMat, Pass::Opaque);
    }
}

void LoadedHelicopter::drawLivery(Shader& shader, const mat4& root) const {
    /* Passe transparente : marquages de livrée (posés sur la coque opaque) puis
       vitrages, dessinés en dernier. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    if (m_gendarmerie) {
        /* "GENDARMERIE" sur le bas des flancs de cabine, "F-BRHP" sur le caisson
           arrière. Positions en coordonnées FlightGear, affinées visuellement.
           Liséré blanc juste au-dessus du mot, sur chaque flanc. */
        drawDecal(shader, root, m_decalStripe, vec3{-3.60f, 0.90f, -0.71f}, 1.15f, 0.015f);
        drawDecal(shader, root, m_decalStripe, vec3{-3.60f, -0.90f, -0.71f}, 1.15f, 0.015f);
        drawDecal(shader, root, m_decalGendarmerie, vec3{-3.60f, 0.90f, -0.80f}, 0.90f, 0.13f);
        drawDecal(shader, root, m_decalGendarmerie, vec3{-3.60f, -0.90f, -0.80f}, 0.90f, 0.13f);
        drawDecal(shader, root, m_decalReg, vec3{-1.92f, 0.62f, -0.05f}, 0.36f, 0.11f);
        drawDecal(shader, root, m_decalReg, vec3{-1.92f, -0.62f, -0.05f}, 0.36f, 0.11f);
    }
    drawModel(shader, m_fuselage, root, Pass::Transparent);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    /* (Un disque flou translucide remplaçant les pales distinctes à haut régime,
       pour éviter l'effet stroboscopique, reste à étudier ; voir l'historique git
       pour une ébauche, ainsi que mainHubWorld() qui en pose le centre.) */
}

}  /* namespace artouste::render */
