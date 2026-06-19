#include "render/Model.hpp"

#include "render/Shader.hpp"

namespace artouste::render {

const Texture* Model::acquireTexture(const std::filesystem::path& path) {
    const std::string key = path.string();
    if (auto it = m_textures.find(key); it != m_textures.end()) {
        return it->second.get();
    }
    auto           texture = std::make_unique<Texture>(path);
    const Texture* raw     = texture.get();
    m_textures.emplace(key, std::move(texture));
    return raw;
}

void Model::addPart(Mesh&& mesh, const Texture* texture, bool transparent, float opacity) {
    m_parts.push_back(Part{std::move(mesh), texture, transparent, opacity});
}

void Model::draw(Shader& shader, Pass pass) const {
    for (const Part& part : m_parts) {
        if (pass == Pass::Opaque && part.transparent) {
            continue;
        }
        if (pass == Pass::Transparent && !part.transparent) {
            continue;
        }
        shader.setFloat("u_opacity", part.opacity);
        if (part.texture != nullptr) {
            part.texture->bind(0);
        }
        part.mesh.draw();
    }
}

}  // namespace artouste::render
