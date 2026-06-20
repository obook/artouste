/*
 * Model.hpp
 * Modèle 3D chargé depuis un fichier : une liste de parties, chacune associant
 * un maillage et une texture. Le modèle possède toutes ses textures (rangées
 * dans un cache par chemin) ; les parties ne font qu'y pointer.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Mesh.hpp"
#include "render/Texture.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace artouste::render {

class Shader;

/* Choix des parties à dessiner. On dessine les opaques d'abord, puis les
   translucides : c'est l'ordre correct pour que la transparence (blending)
   se mélange bien avec ce qui est derrière. */
enum class Pass { Opaque, Transparent, All };

class Model {
public:
    Model() = default;

    /* Dessine les parties retenues par la passe demandée. L'appelant a déjà
       fixé la matrice u_model ; on règle ici, pour chaque partie, son opacité
       (u_opacity) et sa texture (unité 0). 'opacityScale' multiplie l'opacité de
       chaque partie, pour faire apparaitre un modèle en fondu (le disque flou du
       rotor selon le régime, par exemple). */
    void draw(Shader& shader, Pass pass = Pass::All, float opacityScale = 1.0f) const;

    [[nodiscard]] bool        empty() const noexcept { return m_parts.empty(); }
    [[nodiscard]] std::size_t partCount() const noexcept { return m_parts.size(); }

    /* Méthodes de construction, appelées par ModelLoader pendant le chargement. */

    /* Renvoie la texture du chemin donné, en la chargeant une seule fois :
       si elle est déjà dans le cache, on réutilise celle-ci. */
    const Texture* acquireTexture(const std::filesystem::path& path);

    /* Remplace, à l'affichage, la texture de toutes les parties texturées par
       celle-ci (livrée de rechange). Passer nullptr pour revenir aux textures
       d'origine. Les parties sans texture (le vitrage) ne sont pas touchées. */
    void setLivery(const Texture* texture) noexcept { m_livery = texture; }
    /* Ajoute une partie au modèle (maillage + texture + réglages de rendu). */
    void addPart(Mesh&& mesh, const Texture* texture, bool transparent = false,
                 float opacity = 1.0f);

    /* Mémorise les positions des sommets (repère local du modèle), pendant le
       chargement. Sert à retrouver le point de jonction réel entre deux pièces
       (le coude, le poignet), en cherchant la paire de sommets la plus proche. */
    void recordVertices(const std::vector<Vertex>& vertices);
    [[nodiscard]] const std::vector<vec3>& positions() const noexcept { return m_positions; }

private:
    /* Une partie du modèle : un morceau de géométrie avec sa texture et son
       réglage de transparence. */
    struct Part {
        Mesh           mesh;
        const Texture* texture     = nullptr;
        bool           transparent = false;
        float          opacity     = 1.0f;
    };

    std::vector<Part>                                        m_parts;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
    const Texture*                                           m_livery = nullptr;
    std::vector<vec3>                                        m_positions;
};

}  /* namespace artouste::render */
