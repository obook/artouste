/*
 * ExplosionFx.hpp
 * Rendu des explosions 3D animées du mode zombie (impact des roquettes), à la
 * place des simples boules de feu en sphères lumineuses. S'appuie sur
 * render::ExplosionModel (séquence d'images animées par nœuds) et sur le
 * shader explosion (texturé, émissif, additif).
 *
 * Peu d'explosions simultanées : pas d'instanciation, on redessine le modèle
 * une fois par explosion active, chacune à sa propre progression d'animation.
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
    /* Charge le modèle d'explosion. worldRadius est le rayon (m) auquel la boule
       de feu est mise à l'échelle dans le monde. L'appelant fait défiler progress
       de 0 à 1 sur playSpanS secondes (durée de vie réelle de l'explosion, voir
       RocketSystem::EXPLOSION_DURATION_S) ; animStartS permet de démarrer là où
       la boule de feu est DÉJÀ formée, pour qu'elle soit franche dès l'impact
       plutôt que de se construire en retard. animSpeed (1.0 = vitesse native du
       flipbook) accélère la lecture INTERNE du clip par rapport à playSpanS :
       utile quand le clip source a un cadencement natif bas (quelques images
       par seconde, flipbook saccadé) et qu'on veut en montrer davantage dans le
       même temps de vie -- sans quoi playSpanS secondes de vie ne parcourent
       que playSpanS secondes de clip, donc peu d'images distinctes si le clip
       est plus long que la vie de l'explosion. Absent/illisible : built() reste
       faux. */
    ExplosionFx(const std::filesystem::path& modelPath, float worldRadius, float animStartS,
                float playSpanS, float animSpeed = 1.0f);

    [[nodiscard]] bool built() const noexcept { return m_model.built(); }

    /* Une explosion à dessiner : centre monde et progression 0..1 dans
       l'animation (0 = début, 1 = fin). */
    struct Instance {
        vec3  center{0.0f};
        float progress = 0.0f;
    };

    /* Dessine chaque explosion. Le shader doit être en cours d'usage, ses
       uniformes de vue/projection/brume déjà réglés ; draw règle u_model par
       image et gère l'état de mélange (additif, sans écriture de profondeur).
       toRel recale du repère monde vers le repère caméra (translation
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
