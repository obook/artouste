/*
 * Texture.hpp
 * Texture 2D OpenGL chargée depuis un fichier image (PNG via stb_image).
 * La classe possède l'identifiant GL et le libère automatiquement : elle ne
 * peut donc pas être copiée, seulement déplacée (move-only).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

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

    /* Active cette texture sur l'unité de texture donnée (0 par défaut),
       pour que le shader puisse l'échantillonner. */
    void bind(unsigned int unit = 0) const;

    [[nodiscard]] bool         valid() const noexcept { return m_id != 0; }
    [[nodiscard]] unsigned int id() const noexcept { return m_id; }

private:
    unsigned int m_id = 0;
};

}  /* namespace artouste::render */
