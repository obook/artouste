/*
 * LoadedHelicopterInstruments.cpp
 * Les six instruments animés du tableau de bord (horizon artificiel, altimètre,
 * vario, compas, anémomètre, couplemètre) : chargement de chaque cadran en cadran
 * fixe plus parties mobiles (loadInstruments), puis animation au régime de vol
 * (drawInstruments). Reproduit les animations des fichiers .xml de FlightGear.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/LoadedHelicopter.hpp"

#include "render/LoadedHelicopterDetail.hpp"
#include "render/Shader.hpp"

#include <cmath>

namespace artouste::render {

using namespace heli_detail;

void LoadedHelicopter::loadInstruments(const std::filesystem::path& dir) {
    /* Horizon artificiel animé : on charge ai.ac en trois morceaux par filtrage des
       objets (voir ai.xml). On garde, pour chaque morceau, uniquement les objets
       voulus en écartant tous les autres. */
    {
        const vec3                  aiFgOffset{-0.231f, 0.060f, 0.112f};
        const std::filesystem::path aiFile = dir / "Interior/Panel/Instruments/ai/ai.ac";
        m_aiOffset = PANEL_OFFSET + fgToAssimp(aiFgOffset);
        /* Statique (lunette + symbole avion + repères fixes) : on écarte les objets
           mobiles (carte de roulis et barre d'horizon) et le verre. */
        m_aiStatic = loadPart(aiFile, {"background", "scale", "float", "vitre", "blur", "disc"});
        /* Carte de roulis : on ne garde que background et scale. */
        m_aiCard   = loadPart(
            aiFile, {"bouton", "face", "float", "fond", "markings", "vitre", "blur", "disc"});
        /* Barre d'horizon : on ne garde que float. */
        m_aiFloat  = loadPart(
            aiFile,
            {"background", "bouton", "face", "fond", "markings", "scale", "vitre", "blur", "disc"});
        m_hasAi = !m_aiStatic.empty() || !m_aiCard.empty() || !m_aiFloat.empty();
    }

    /* Altimètre animé : on charge alt.ac en quatre morceaux (cadran + 3 aiguilles).
       Les noms needle100 / needle1000 / needle10000 se contiennent en sous-chaîne :
       on les isole donc par NOM EXACT (préfixe '=' du filtre). */
    {
        const vec3                  altFgOffset{-0.222f, -0.181f, 0.112f};
        const std::filesystem::path altFile = dir / "Interior/Panel/Instruments/alt/alt.ac";
        m_altOffset = PANEL_OFFSET + fgToAssimp(altFgOffset);
        /* Cadran statique : on écarte les 3 aiguilles (par nom exact) et le verre. */
        m_altStatic = loadPart(
            altFile, {"=needle100", "=needle1000", "=needle10000", "vitre", "blur", "disc"});
        /* Aiguille des centaines : on garde needle100, on écarte les deux autres. */
        m_altN100 = loadPart(altFile, {"fond", "button", "face", "inhg", "=needle1000",
                                       "=needle10000", "vitre", "blur", "disc"});
        /* Aiguille des milliers. */
        m_altN1000 = loadPart(altFile, {"fond", "button", "face", "inhg", "=needle100",
                                        "=needle10000", "vitre", "blur", "disc"});
        /* Aiguille des dizaines de milliers. */
        m_altN10000 = loadPart(altFile, {"fond", "button", "face", "inhg", "=needle100",
                                         "=needle1000", "vitre", "blur", "disc"});
        m_hasAlt = !m_altN100.empty() || !m_altN1000.empty() || !m_altN10000.empty();
    }

    /* Vario (VSI) animé : on charge vsi.ac en deux morceaux. L'aiguille est l'objet
       "needle"; le reste (face, fond, vitre) forme le cadran fixe. */
    {
        const vec3                  vsiFgOffset{-0.208f, -0.060f, 0.112f};
        const std::filesystem::path vsiFile = dir / "Interior/Panel/Instruments/vsi/vsi.ac";
        m_vsiOffset = PANEL_OFFSET + fgToAssimp(vsiFgOffset);
        /* Cadran : on garde face + fond, on écarte l'aiguille et le verre "vitre"
           (opaque, il apparaitrait comme un disque noir, comme pour l'altimètre). */
        m_vsiStatic = loadPart(vsiFile, {"needle", "vitre"});
        m_vsiNeedle = loadPart(vsiFile, {"face", "fond", "vitre"});   /* l'aiguille seule */
        m_hasVsi    = !m_vsiNeedle.empty();
    }

    /* Compas (conservateur de cap) animé : on charge hi.ac en deux morceaux. La rose
       des vents mobile est l'objet "face"; le reste (lunette "fond", symbole avion et
       index "front", boutons, bug de cap) est fixe. On écarte le verre "vitre" (opaque,
       il apparaitrait comme un disque noir, comme pour le vario). */
    {
        const vec3                  hiFgOffset{-0.223f, -0.060f, -0.014f};
        const std::filesystem::path hiFile = dir / "Interior/Panel/Instruments/hi/hi.ac";
        m_hiOffset = PANEL_OFFSET + fgToAssimp(hiFgOffset);
        /* Cadran fixe : tout sauf la rose mobile et le verre. */
        m_hiStatic = loadPart(hiFile, {"face", "vitre"});
        /* Rose des vents mobile : la "face" seule. */
        m_hiCard = loadPart(hiFile, {"fond", "front", "vitre", "Hdg-Knob", "HdgBug", "OBS-Knob"});
        m_hasHi  = !m_hiCard.empty();
    }

    /* Anémomètre (ASI) animé : on charge asi.ac en deux morceaux (cadran fixe +
       aiguille "needle"). Verre "vitre" exclu (opaque, disque noir sinon). */
    {
        const vec3                  asiFgOffset{-0.207f, 0.181f, 0.112f};
        const std::filesystem::path asiFile = dir / "Interior/Panel/Instruments/asi/asi.ac";
        m_asiOffset = PANEL_OFFSET + fgToAssimp(asiFgOffset);
        m_asiStatic = loadPart(asiFile, {"needle", "vitre"});         /* face + fond */
        m_asiNeedle = loadPart(asiFile, {"face", "fond", "vitre"});   /* l'aiguille seule */
        m_hasAsi    = !m_asiNeedle.empty();
    }

    /* Couplemètre (torque) animé : cadran fixe + aiguille "needle"; verre exclu. */
    {
        const vec3                  torqueFgOffset{-0.223f, 0.181f, -0.014f};
        const std::filesystem::path torqueFile =
            dir / "Interior/Panel/Instruments/torque/torque.ac";
        m_torqueOffset = PANEL_OFFSET + fgToAssimp(torqueFgOffset);
        m_torqueStatic = loadPart(torqueFile, {"needle", "vitre"});        /* face + fond */
        m_torqueNeedle = loadPart(torqueFile, {"face", "fond", "vitre"});  /* l'aiguille seule */
        m_hasTorque    = !m_torqueNeedle.empty();
    }
}

void LoadedHelicopter::drawInstruments(Shader& shader, const mat4& root, float rollRad,
                                       float pitchRad, float altitudeFt, float varioFpm,
                                       float headingRad, float airspeedKt, float torquePct) const {
    drawModel(shader, m_panel, root * glm::translate(mat4(1.0f), PANEL_OFFSET), Pass::Opaque);
    for (const Gauge& gauge : m_gauges) {
        drawModel(shader, gauge.model, root * glm::translate(mat4(1.0f), gauge.offset),
                  Pass::Opaque);
    }

    /* Horizon artificiel animé (d'après ai.xml de FlightGear). Roulis : la carte
       (sky/sol) et la barre tournent autour de X. Tangage : seule la barre se
       translate verticalement (comme FlightGear), la carte reste en place. Ce cadran
       est un disque plat (un vrai horizon est une sphère) : un grand déplacement
       laisserait déborder le ciel sur le sol ou découvrirait le fond noir. On garde
       donc un tangage discret (facteur -0.0004 m/deg, borné a +-0.008 m). Le repère
       de la géométrie chargée a la verticale en Y (assimp réoriente le .ac). Le
       symbole avion et la lunette (m_aiStatic) restent fixes. */
    if (m_hasAi) {
        const mat4  aiBase = root * glm::translate(mat4(1.0f), m_aiOffset);
        const mat4  rollM  = aiBase * glm::rotate(mat4(1.0f), rollRad, vec3{1.0f, 0.0f, 0.0f});
        const float pitchOffset =
            glm::clamp(-0.0004f * glm::degrees(pitchRad), -0.008f, 0.008f);
        const mat4 floatM = rollM * glm::translate(mat4(1.0f), vec3{0.0f, pitchOffset, 0.0f});
        drawModel(shader, m_aiStatic, aiBase, Pass::Opaque);
        drawModel(shader, m_aiCard, rollM, Pass::Opaque);
        drawModel(shader, m_aiFloat, floatM, Pass::Opaque);
    }

    /* Altimètre animé (d'après alt.xml) : les trois aiguilles tournent autour de X
       avec l'altitude (en pieds), aux facteurs FlightGear : 0.36 deg/ft (centaines,
       un tour par 1000 ft), 0.036 (milliers) et 0.0036 (dizaines de milliers). */
    if (m_hasAlt) {
        const mat4 altBase = root * glm::translate(mat4(1.0f), m_altOffset);
        const auto needleM = [&](float factor) {
            return altBase *
                   glm::rotate(mat4(1.0f), glm::radians(factor * altitudeFt), vec3{-1.0f, 0.0f, 0.0f});
        };
        drawModel(shader, m_altStatic, altBase, Pass::Opaque);
        drawModel(shader, m_altN10000, needleM(0.0036f), Pass::Opaque);
        drawModel(shader, m_altN1000, needleM(0.036f), Pass::Opaque);
        drawModel(shader, m_altN100, needleM(0.36f), Pass::Opaque);
    }

    /* Vario (VSI) animé : l'aiguille tourne autour de X selon la vitesse verticale,
       suivant la table non linéaire relevée sur le cadran (vsiNeedleAngleDeg). Le
       calage du zéro (VSI_ZERO_OFFSET_DEG) et le sens de rotation sont a confirmer
       en vol, comme cela a été fait pour l'altimètre. */
    if (m_hasVsi) {
        constexpr float VSI_ZERO_OFFSET_DEG = 0.0f;
        const mat4      vsiBase = root * glm::translate(mat4(1.0f), m_vsiOffset);
        const float     angle   = vsiNeedleAngleDeg(varioFpm) + VSI_ZERO_OFFSET_DEG;
        const mat4      needleMat =
            vsiBase * glm::rotate(mat4(1.0f), glm::radians(angle), vec3{-1.0f, 0.0f, 0.0f});
        drawModel(shader, m_vsiStatic, vsiBase, Pass::Opaque);
        drawModel(shader, m_vsiNeedle, needleMat, Pass::Opaque);
    }

    /* Compas animé : la rose des vents (m_hiCard) tourne autour de X avec le cap, de
       sorte que le cap courant vienne sous l'index fixe (le symbole avion m_hiStatic
       ne bouge pas). Le sens et le calage du Nord sont a confirmer en vol (constante
       HI_ZERO_OFFSET_DEG), comme l'altimètre et le vario. */
    if (m_hasHi) {
        constexpr float HI_ZERO_OFFSET_DEG = 0.0f;
        const mat4      hiBase = root * glm::translate(mat4(1.0f), m_hiOffset);
        const float     angle  = -glm::degrees(headingRad) + HI_ZERO_OFFSET_DEG;
        const mat4      cardMat =
            hiBase * glm::rotate(mat4(1.0f), glm::radians(angle), vec3{-1.0f, 0.0f, 0.0f});
        drawModel(shader, m_hiStatic, hiBase, Pass::Opaque);
        drawModel(shader, m_hiCard, cardMat, Pass::Opaque);
    }

    /* Anémomètre animé : l'aiguille tourne autour de X proportionnellement à la vitesse
       air. La face est linéaire (mesurée sur asi.png) : ~1.75 deg/kt. Le sens et le
       calage du zéro (ASI_ZERO_OFFSET_DEG) sont a confirmer en vol, comme les autres. */
    if (m_hasAsi) {
        constexpr float ASI_DEG_PER_KT      = 1.75f;
        constexpr float ASI_ZERO_OFFSET_DEG = 0.0f;
        const float     kt      = airspeedKt < 0.0f ? 0.0f : (airspeedKt > 150.0f ? 150.0f : airspeedKt);
        const mat4      asiBase = root * glm::translate(mat4(1.0f), m_asiOffset);
        const float     angle   = ASI_DEG_PER_KT * kt + ASI_ZERO_OFFSET_DEG;
        const mat4      needleMat =
            asiBase * glm::rotate(mat4(1.0f), glm::radians(angle), vec3{-1.0f, 0.0f, 0.0f});
        drawModel(shader, m_asiStatic, asiBase, Pass::Opaque);
        drawModel(shader, m_asiNeedle, needleMat, Pass::Opaque);
    }

    /* Couplemètre animé : l'aiguille tourne autour de X selon le couple estimé (couple%
       = collectif * 100 * fraction rotor, calculé par l'appelant). La face est linéaire
       (mesurée sur torque.png) : ~2.73 deg/%. Sens et calage du zéro a confirmer en vol. */
    if (m_hasTorque) {
        constexpr float TORQUE_DEG_PER_PCT     = 2.73f;
        constexpr float TORQUE_ZERO_OFFSET_DEG = 0.0f;
        const float     pct = torquePct < 0.0f ? 0.0f : (torquePct > 110.0f ? 110.0f : torquePct);
        const mat4      torqueBase = root * glm::translate(mat4(1.0f), m_torqueOffset);
        const float     angle      = TORQUE_DEG_PER_PCT * pct + TORQUE_ZERO_OFFSET_DEG;
        const mat4      needleMat =
            torqueBase * glm::rotate(mat4(1.0f), glm::radians(angle), vec3{-1.0f, 0.0f, 0.0f});
        drawModel(shader, m_torqueStatic, torqueBase, Pass::Opaque);
        drawModel(shader, m_torqueNeedle, needleMat, Pass::Opaque);
    }
}

}  /* namespace artouste::render */
