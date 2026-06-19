// Modèle chargé depuis un fichier : une liste de parties (maillage + texture).
// Le modèle possède ses textures (cache par chemin), les parties y réfèrent.

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

// Quelles parties dessiner : opaques d'abord, translucides ensuite (blending).
enum class Pass { Opaque, Transparent, All };

class Model {
public:
    Model() = default;

    // Dessine les parties correspondant à la passe. L'appelant a déjà positionné
    // u_model ; on règle ici u_opacity et la texture (unité 0) par partie.
    void draw(Shader& shader, Pass pass = Pass::All) const;

    [[nodiscard]] bool        empty() const noexcept { return m_parts.empty(); }
    [[nodiscard]] std::size_t partCount() const noexcept { return m_parts.size(); }

    // Construction (utilisée par ModelLoader).
    const Texture* acquireTexture(const std::filesystem::path& path);
    void addPart(Mesh&& mesh, const Texture* texture, bool transparent = false,
                 float opacity = 1.0f);

private:
    struct Part {
        Mesh           mesh;
        const Texture* texture     = nullptr;
        bool           transparent = false;
        float          opacity     = 1.0f;
    };

    std::vector<Part>                                        m_parts;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
};

}  // namespace artouste::render
