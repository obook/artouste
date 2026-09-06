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

void LoadedHelicopter::drawRotors(Shader& shader, const mat4& root, float rotorAngle,
                                  float rotorFraction) const {
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

    /* Position d'une pale du rotor de queue, même principe. */
    const auto tailBladeMat = [&](int k) {
        const float heading =
            static_cast<float>(k) * (TWO_PI / static_cast<float>(TAIL_BLADES));
        return tailBase * glm::rotate(mat4(1.0f), heading, vec3{0.0f, 1.0f, 0.0f}) *
               glm::translate(mat4(1.0f), vec3{0.0f, TAIL_BLADE_RISE, 0.0f});
    };

    drawModel(shader, m_mainHub, mainBase, Pass::Opaque);
    drawModel(shader, m_tailHub, tailBase, Pass::Opaque);

    /* Rotor lent ou arrêté : les pales nettes, opaques. */
    const float blur = blurFade(rotorFraction);
    if (blur <= 0.0f) {
        for (int k = 0; k < MAIN_BLADES; ++k) {
            drawModel(shader, m_mainBlade, mainBladeMat(k), Pass::Opaque);
        }
        for (int k = 0; k < TAIL_BLADES; ++k) {
            drawModel(shader, m_tailBlade, tailBladeMat(k), Pass::Opaque);
        }
        return;
    }

    /* En régime : les pales s'effacent, les plans flous du modèle prennent leur
       place. Les deux se dessinent en mélange, profondeur en lecture seule pour
       qu'un plan n'en cache pas un autre. */
    const float sharp   = 1.0f - blur;
    const float opacity = blur * BLUR_OPACITY;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (int k = 0; k < MAIN_BLADES; ++k) {
        if (sharp > 0.0f) {
            drawModel(shader, m_mainBlade, mainBladeMat(k), Pass::Opaque, sharp);
        }
        drawModel(shader, m_mainBlur, mainBladeMat(k), Pass::Opaque, opacity);
    }
    for (int k = 0; k < TAIL_BLADES; ++k) {
        if (sharp > 0.0f) {
            drawModel(shader, m_tailBlade, tailBladeMat(k), Pass::Opaque, sharp);
        }
        drawModel(shader, m_tailBlur, tailBladeMat(k), Pass::Opaque, opacity);
    }
    /* Secteurs de disque, répétés pour faire le tour complet des deux rotors. */
    for (int k = 0; k < DISC_COPIES; ++k) {
        const float heading =
            static_cast<float>(k) * (TWO_PI / static_cast<float>(DISC_COPIES));
        drawModel(shader, m_mainDisc,
                  mainBase * glm::rotate(mat4(1.0f), heading, vec3{0.0f, 1.0f, 0.0f}) *
                      glm::translate(mat4(1.0f), vec3{0.0f, MAIN_BLADE_RISE, 0.0f}),
                  Pass::Opaque, opacity);
        drawModel(shader, m_tailDisc,
                  tailBase * glm::rotate(mat4(1.0f), heading, vec3{0.0f, 1.0f, 0.0f}) *
                      glm::translate(mat4(1.0f), vec3{0.0f, TAIL_BLADE_RISE, 0.0f}),
                  Pass::Opaque, opacity);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
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
}

} /* namespace artouste::render */
