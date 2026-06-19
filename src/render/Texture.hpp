// Texture 2D OpenGL chargée depuis un fichier image (PNG via stb_image).
// RAII sur l'identifiant GL (move-only), comme le reste des ressources GPU.

#pragma once

#include <filesystem>

namespace artouste::render {

class Texture {
public:
    Texture() = default;
    explicit Texture(const std::filesystem::path& path);
    ~Texture();

    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Lie la texture à l'unité de texture donnée (0 par défaut).
    void bind(unsigned int unit = 0) const;

    [[nodiscard]] bool         valid() const noexcept { return m_id != 0; }
    [[nodiscard]] unsigned int id() const noexcept { return m_id; }

private:
    unsigned int m_id = 0;
};

}  // namespace artouste::render
