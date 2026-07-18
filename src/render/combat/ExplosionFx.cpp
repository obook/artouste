/*
 * ExplosionFx.cpp
 * Voir ExplosionFx.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "render/combat/ExplosionFx.hpp"

#include "render/Shader.hpp"
#include "render/Texture.hpp"

#include <glad/glad.h>

namespace artouste::render {

ExplosionFx::ExplosionFx(const std::filesystem::path& modelPath, float worldRadius, float animStartS,
                         float playSpanS)
    : m_model(modelPath), m_worldRadius(worldRadius), m_animStartS(animStartS),
      m_playSpanS(playSpanS) {}

void ExplosionFx::draw(Shader& shader, const mat4& toRel, const std::vector<Instance>& explosions) {
    if (!m_model.built() || explosions.empty()) {
        return;
    }
    if (m_model.texture() != nullptr) {
        m_model.texture()->bind(0);
    }

    /* Mélange additif (feu émissif) : les zones sombres de la texture n'ajoutent
       rien, les zones vives éclaircissent la scene. On lit la profondeur (occlusion
       par le relief/l'appareil) sans l'écrire (les explosions ne masquent rien). */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    for (const Instance& ex : explosions) {
        const float p = clamp(ex.progress, 0.0f, 1.0f);
        /* Temps d'animation a VITESSE REELLE, démarré ou la boule de feu est deja
           formée (animStartS) : franc des l'impact, pas de construction en
           retard. */
        const float             t       = m_animStartS + p * m_playSpanS;
        const std::vector<mat4> globals = m_model.poseAtTime(t);

        /* Croissance continue tres légere et enveloppe de fondu : apparition
           quasi immédiate (la boule est deja grosse), extinction seulement en
           toute fin (l'animation du pack se dissipe déja d'elle-meme ; le fondu
           n'évite que la coupure nette des dernieres images encore actives). */
        const float growth  = 0.95f + 0.15f * p;
        const float fadeIn  = glm::smoothstep(0.0f, 0.04f, p);
        const float fadeOut = 1.0f - glm::smoothstep(0.80f, 1.0f, p);
        shader.setFloat("u_fade", fadeIn * fadeOut);

        /* Placement commun : recalage caméra, position d'impact, rayon monde,
           puis normalisation du modele (localFix). La transformation animée du
           noeud de chaque image s'ajoute ensuite (échelle du flipbook). */
        const mat4 base = toRel * glm::translate(mat4(1.0f), ex.center) *
                          glm::scale(mat4(1.0f), vec3{m_worldRadius * growth}) * m_model.localFix();

        for (std::size_t i = 0; i < m_model.partCount(); ++i) {
            const ExplosionModel::MeshPart& part = m_model.part(i);
            shader.setMat4("u_model", base * globals[static_cast<std::size_t>(part.node)]);
            part.mesh.draw();
        }
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

}  /* namespace artouste::render */
