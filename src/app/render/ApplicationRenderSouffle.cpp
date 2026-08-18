/*
 * ApplicationRenderSouffle.cpp
 * Souffle rotor : la poussière soulevée au ras du sol, sa simulation et son
 * dessin.
 *
 * La simulation (app::SouffleRotor) est indépendante du rendu
 * (render::SouffleFx) : ce fichier ne fait que les brancher l'une à l'autre.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "render/LoadedHelicopter.hpp"
#include "render/Shader.hpp"
#include "render/SouffleFx.hpp"
#include "render/Terrain.hpp"

#include <glad/glad.h>

#include <cmath>

namespace artouste::app {

void Application::updateSouffle(const mat4& base, float rotorFraction, float collective, float dt) {
    if (!m_terrain) {
        return;
    }
    /* Origine du souffle : l'axe du mât, en avant du centre de l'appareil (le
       même décalage que l'ombre portée, voir drawGroundShadow). Sans le modèle
       chargé, on retombe sur le centre. */
    const float avance = m_loadedHeli ? render::LoadedHelicopter::ROTOR_FORWARD_OFFSET : 0.0f;
    const vec3  centreRotor = vec3(base * vec4(avance, 0.0f, 0.0f, 1.0f));
    m_souffle.update(dt, centreRotor, rotorFraction, collective, [this](float x, float z) {
        return m_terrain->heightAt(x, z);
    });
}

void Application::drawSouffle(const RenderContext& ctx) {
    if (!m_souffleFx || !m_souffleShader) {
        return;
    }
    const std::vector<SouffleRotor::Bouffee>& bouffees = m_souffle.bouffees();
    if (bouffees.empty()) {
        return;
    }

    std::vector<render::SouffleFx::Instance> instances;
    instances.reserve(bouffees.size());
    for (const SouffleRotor::Bouffee& b : bouffees) {
        render::SouffleFx::Instance inst;
        inst.centreDiametre = vec4{b.centre, b.diametre};
        inst.grain          = vec4{b.opacite, b.graine, b.rotation, b.hauteurSol};
        instances.push_back(inst);
    }
    m_souffleFx->updateInstances(instances);

    m_souffleShader->use();
    m_souffleShader->setMat4("u_model", ctx.toRel);
    m_souffleShader->setMat4("u_view", ctx.view);
    m_souffleShader->setMat4("u_proj", ctx.proj);
    m_souffleShader->setVec3("u_lightDir", ctx.lightDir);
    m_souffleShader->setVec3("u_camPos", ctx.camPosRel);
    m_souffleShader->setVec3("u_fogColor", ctx.fogColor);
    m_souffleShader->setFloat("u_fogStart", m_fogStart);
    m_souffleShader->setFloat("u_fogEnd", m_fogEnd);
    m_souffleShader->setInt("u_ortho", 0);

    /* Couleur du sol : le shader prélève l'orthophoto sous chaque bouffée. Le
       calage reprend celui du maillage du terrain (coin nord-ouest et emprise).
       Une emprise nulle (carte sans orthophoto, terrain de repli) fait retomber
       le shader sur une teinte de poussière par défaut. */
    if (m_terrain->textured()) {
        m_terrain->bindTexture(0);
        m_souffleShader->setVec2("u_orthoMin",
                                 vec2{m_terrain->originX() - m_terrain->halfWidth(),
                                      m_terrain->originZ() - m_terrain->halfHeight()});
        m_souffleShader->setVec2(
            "u_orthoTaille",
            vec2{2.0f * m_terrain->halfWidth(), 2.0f * m_terrain->halfHeight()});
    } else {
        m_souffleShader->setVec2("u_orthoTaille", vec2{0.0f, 0.0f});
    }

    /* Mélange alpha classique : la poussière absorbe la lumière, elle ne l'ajoute
       pas (contrairement aux lueurs moteur ci-dessus). On lit la profondeur sans
       l'écrire : le relief et l'appareil masquent le nuage, le nuage ne masque
       rien. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    m_souffleFx->draw();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} /* namespace artouste::app */
