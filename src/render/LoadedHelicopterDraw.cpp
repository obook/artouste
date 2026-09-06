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
                            float rotorFraction, bool fullPilot, float rudder, float cyclicLong,
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
    drawRotors(shader, root, rotorAngle, rotorFraction);
    drawLivery(shader, root);
}

void LoadedHelicopter::drawAirframe(Shader& shader, const mat4& root, bool fullPilot,
                                    float rudder, float cyclicLong, float cyclicLat,
                                    float collective) const {
    /* Fuselage (hors vitrages), intérieur et les deux occupants. Les pales du rotor
       principal sont traitées à part, en mélange, pour le fondu selon le régime. */
    drawModel(shader, m_fuselage, root * m_fuselageFix, Pass::Opaque);
    drawModel(shader, m_interior, root, Pass::Opaque);
    /* Arceau de protection du rotor de queue (jaune en livrée Gendarmerie). Il a
       été retiré du fuselage (alouette.ac) et rechargé en pièce séparée, donc il se
       dessine simplement à sa place, sans z-fighting. */
    drawModel(shader, m_tailGuard, root, Pass::Opaque);
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

        /* Cible de prise = pommeau du levier, reculé de GRIP_DROP LE LONG DE LA TIGE
           (et non à la verticale du monde) : la main saisit la poignée juste sous le
           pommeau et y reste quel que soit l'angle du levier. Un simple décalage en Y
           tombait à côté de la tige à collectif bas (levier plus couché), d'où la main
           qui décrochait. La direction "vers la base" est l'axe +x local du levier
           transformé dans le monde ; le pivot du levier (sa base) et celui de la main
           (le coude) diffèrent, mais la cible restant sur la tige, la prise tient. */
        constexpr float GRIP_DROP = 0.05f;  /* m, recul de la prise sous le pommeau */
        const mat4 leverWorld = colAt * leverLocal;
        const vec3 alongShaft = glm::normalize(mat3(leverWorld) * vec3{1.0f, 0.0f, 0.0f});
        vec3       gripModel   = vec3(leverWorld * vec4(m_collectiveGripLocal, 1.0f));
        gripModel += alongShaft * GRIP_DROP;
        const vec3 elbow = s.pilot + m_elbowLeftLocal;
        const vec3 hand0 = s.pilot + m_handLeftLocal;          /* main au repos */
        drawModel(shader, m_forearmLeft,
                  forearmTransform(pilotBaseL, m_elbowLeftLocal, elbow, hand0, gripModel),
                  Pass::Opaque);
    }
}

} /* namespace artouste::render */
