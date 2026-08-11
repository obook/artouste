/*
 * Model.cpp
 * Implémentation du modèle 3D : cache de textures partagées, ajout des parties
 * et dessin trié par passe (opaque ou transparente).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Model.hpp"

#include "render/Shader.hpp"

namespace artouste::render {

const Texture* Model::acquireTexture(const std::filesystem::path& path) {
    /* Si cette texture a déjà été chargée, on renvoie la même : inutile de la
       relire depuis le disque et de la dupliquer en mémoire GPU. */
    const std::string key = path.string();
    if (auto it = m_textures.find(key); it != m_textures.end()) {
        return it->second->valid() ? it->second.get() : nullptr;
    }
    /* Premier usage de ce chemin : on charge la texture et on la met en cache.
       Un fichier absent ou illisible donne nullptr, pas une texture vide : les
       appelants traitent nullptr comme "garder la texture d'origine", alors
       qu'une texture ratée peindrait la pièce en noir. On la garde tout de même
       en cache pour ne pas retenter la lecture à chaque appel. */
    auto           texture = std::make_unique<Texture>(path);
    const Texture* raw     = texture->valid() ? texture.get() : nullptr;
    m_textures.emplace(key, std::move(texture));
    return raw;
}

const Texture* Model::acquireEmbeddedTexture(const std::string& key, const unsigned char* data,
                                             std::size_t size) {
    if (auto it = m_textures.find(key); it != m_textures.end()) {
        return it->second.get();
    }
    auto           texture = std::make_unique<Texture>(data, size);
    const Texture* raw     = texture.get();
    m_textures.emplace(key, std::move(texture));
    return raw;
}

void Model::addPart(Mesh&& mesh, const Texture* texture, bool transparent, float opacity,
                    const Texture* relief) {
    m_parts.push_back(Part{std::move(mesh), texture, relief, transparent, opacity});
}

void Model::recordVertices(const std::vector<Vertex>& vertices) {
    m_positions.reserve(m_positions.size() + vertices.size());
    for (const Vertex& v : vertices) {
        m_positions.push_back(v.position);
    }
}

void Model::draw(Shader& shader, Pass pass, float opacityScale) const {
    for (const Part& part : m_parts) {
        /* On saute les parties qui ne correspondent pas à la passe demandée. */
        if (pass == Pass::Opaque && part.transparent) {
            continue;
        }
        if (pass == Pass::Transparent && !part.transparent) {
            continue;
        }
        /* On transmet au shader les réglages propres à cette partie, puis on
           dessine son maillage. */
        shader.setFloat("u_opacity", part.opacity * opacityScale);
        /* Livrée de rechange : si elle est posée, elle remplace la texture des
           parties texturées (le vitrage, sans texture, garde son rendu). */
        const Texture* texture = (m_livery != nullptr && part.texture != nullptr)
                                     ? m_livery
                                     : part.texture;
        /* Carte de relief sur l'unité 1. Quand la pièce n'en a pas, on le dit au
           shader plutôt que de laisser en place celle de la pièce précédente.
           La texture de couleur est liée en dernier pour que l'unité 0 reste
           l'unité active à la sortie. */
        shader.setInt("u_hasRelief", part.relief != nullptr ? 1 : 0);
        if (part.relief != nullptr) {
            part.relief->bind(1);
        }
        if (texture != nullptr) {
            texture->bind(0);
        }
        part.mesh.draw();
    }
}

}  /* namespace artouste::render */
