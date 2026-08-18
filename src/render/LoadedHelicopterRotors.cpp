/*
 * LoadedHelicopterRotors.cpp
 * Rotors en mouvement et pièces de livrée de l'appareil.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/LoadedHelicopter.hpp"

#include <glad/glad.h>

#include "render/LoadedHelicopterDetail.hpp"
#include "render/Model.hpp"
#include "render/Shader.hpp"

#include <cmath>

namespace artouste::render {

using namespace heli_detail;

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
    if (m_livery == Livery::Gendarmerie) {
        /* "GENDARMERIE" sur le bas des flancs de cabine, "F-BRHP" sur le caisson
           arrière. Positions en coordonnées FlightGear, affinées visuellement.
           Liséré blanc juste au-dessus du mot, sur chaque flanc. */
        drawDecal(shader, root, m_decalStripe, vec3{-3.60f, 0.90f, -0.71f}, 1.15f, 0.015f);
        drawDecal(shader, root, m_decalStripe, vec3{-3.60f, -0.90f, -0.71f}, 1.15f, 0.015f);
        drawDecal(shader, root, m_decalGendarmerie, vec3{-3.60f, 0.90f, -0.80f}, 0.90f, 0.13f);
        drawDecal(shader, root, m_decalGendarmerie, vec3{-3.60f, -0.90f, -0.80f}, 0.90f, 0.13f);
        drawDecal(shader, root, m_decalReg, vec3{-1.92f, 0.62f, -0.05f}, 0.36f, 0.11f);
        drawDecal(shader, root, m_decalReg, vec3{-1.92f, -0.62f, -0.05f}, 0.36f, 0.11f);
    } else if (m_livery == Livery::ArmeeDeTerre) {
        /* Code d'appareil "341-HN" de l'ALAT sur le caisson arrière, sur chaque
           flanc. La cocarde tricolore est déjà dans l'atlas olive (préservée du
           marquage d'origine), inutile de la poser en décalque. */
        drawDecal(shader, root, m_decalReg341, vec3{-1.92f, 0.62f, -0.05f}, 0.42f, 0.11f);
        drawDecal(shader, root, m_decalReg341, vec3{-1.92f, -0.62f, -0.05f}, 0.42f, 0.11f);
    } else if (m_livery == Livery::ProtectionCivile) {
        /* "PROTECTION CIVILE" en blanc sur le bas des flancs de cabine et le code
           "F-AYEM" sur le caisson arrière, sur chaque flanc. La cocarde tricolore
           est déjà dans l'atlas rouge (préservée du marquage d'origine). */
        drawDecal(shader, root, m_decalProtCiv, vec3{-3.55f, 0.90f, -0.78f}, 1.30f, 0.12f);
        drawDecal(shader, root, m_decalProtCiv, vec3{-3.55f, -0.90f, -0.78f}, 1.30f, 0.12f);
        drawDecal(shader, root, m_decalRegAyem, vec3{-1.92f, 0.62f, -0.05f}, 0.42f, 0.11f);
        drawDecal(shader, root, m_decalRegAyem, vec3{-1.92f, -0.62f, -0.05f}, 0.42f, 0.11f);
    }
    drawModel(shader, m_fuselage, root * m_fuselageFix, Pass::Transparent);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    /* (Un disque flou translucide remplaçant les pales distinctes à haut régime,
       pour éviter l'effet stroboscopique, reste à étudier ; voir l'historique git
       pour une ébauche, ainsi que mainHubWorld() qui en pose le centre.) */
}

} /* namespace artouste::render */
