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
       de feu est mise a l'échelle dans le monde. L'appelant fait défiler progress
       de 0 a 1 sur playSpanS secondes (durée de vie réelle de l'explosion, voir
       RocketSystem::EXPLOSION_DURATION_S) ; animStartS permet de démarrer la ou
       la boule de feu est DEJA formée, pour qu'elle soit franche des l'impact
       plutot que de se construire en retard. animSpeed (1.0 = vitesse native du
       flipbook) accélère la lecture INTERNE du clip par rapport a playSpanS :
       utile quand le clip source a un cadencement natif bas (quelques images
       par seconde, flipbook saccadé) et qu'on veut en montrer davantage dans le
       même temps de vie -- sans quoi playSpanS secondes de vie ne parcourent
       que playSpanS secondes de clip, donc peu d'images distinctes si le clip
       est plus long que la vie de l'explosion. Absent/illisible : built() reste
       faux. */
    ExplosionFx(const std::filesystem::path& modelPath, float worldRadius, float animStartS,
                float playSpanS, float animSpeed = 1.0f);

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
    float          m_animSpeed   = 1.0f;
};

}  /* namespace artouste::render */
