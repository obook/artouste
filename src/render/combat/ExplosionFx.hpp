/*
 * ExplosionFx.hpp
 * Rendu des explosions 3D animées du mode zombie (impact des roquettes), a la
 * place des simples boules de feu en sphères lumineuses. S'appuie sur
 * render::ExplosionModel (séquence d'images animées par noeuds) et sur le
 * shader explosion (texturé, émissif, additif).
 *
 * Peu d'explosions simultanées : pas d'instanciation, on redessine le modele
 * une fois par explosion active, chacune a sa propre progression d'animation.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/ExplosionModel.hpp"
#include "util/Math.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace artouste::render {

class Shader;

class ExplosionFx {
public:
    /* Charge le modele d'explosion. worldRadius est le rayon (m) auquel la boule
       de feu est mise a l'échelle dans le monde. L'animation est jouée a VITESSE
       REELLE de animStartS a animStartS + playSpanS (l'appelant fait défiler
       progress de 0 a 1 sur playSpanS secondes, ni accéléré ni ralenti, sinon le
       flipbook "saute"). animStartS permet de démarrer la ou la boule de feu est
       DEJA formée, pour qu'elle soit franche des l'impact plutot que de se
       construire en retard. Absent/illisible : built() reste faux. */
    ExplosionFx(const std::filesystem::path& modelPath, float worldRadius, float animStartS,
                float playSpanS);

    [[nodiscard]] bool built() const noexcept { return m_model.built(); }

    /* Une explosion a dessiner : centre monde et progression 0..1 dans
       l'animation (0 = début, 1 = fin). */
    struct Instance {
        vec3  center{0.0f};
        float progress = 0.0f;
    };

    /* Dessine chaque explosion. Le shader doit etre en cours d'usage, ses
       uniformes de vue/projection/brume déja réglés ; draw règle u_model par
       image et gere l'état de mélange (additif, sans écriture de profondeur).
       toRel recale du repere monde vers le repere caméra (translation
       -origine de rendu), comme les autres rendus. */
    void draw(Shader& shader, const mat4& toRel, const std::vector<Instance>& explosions);

private:
    ExplosionModel m_model;
    float          m_worldRadius = 1.0f;
    float          m_animStartS  = 0.0f;
    float          m_playSpanS   = 1.0f;
};

}  /* namespace artouste::render */
