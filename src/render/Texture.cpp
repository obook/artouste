#include "render/Texture.hpp"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>
#include <utility>

namespace artouste::render {

Texture::Texture(const std::filesystem::path& path) {
    // stb_image charge l'origine en haut à gauche ; OpenGL attend le bas à
    // gauche, d'où le retournement vertical.
    stbi_set_flip_vertically_on_load(1);

    int width    = 0;
    int height   = 0;
    int channels = 0;
    unsigned char* pixels =
        stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        std::fprintf(stderr, "[Texture] échec du chargement : %s\n", path.string().c_str());
        return;
    }

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);
}

Texture::~Texture() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
    }
}

Texture::Texture(Texture&& other) noexcept : m_id(std::exchange(other.m_id, 0)) {}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) {
            glDeleteTextures(1, &m_id);
        }
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}

void Texture::bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

}  // namespace artouste::render
