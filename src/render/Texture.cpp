/*
 * Texture.cpp
 * Implémentation de la texture OpenGL : chargement du fichier image avec
 * stb_image, envoi des pixels au GPU et gestion du cycle de vie (déplacement,
 * destruction).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/Texture.hpp"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>
#include <utility>

namespace artouste::render {

Texture::Texture(const std::filesystem::path& path) {
    /* stb_image place l'origine de l'image en haut à gauche, alors qu'OpenGL
       l'attend en bas à gauche : on retourne donc l'image verticalement. */
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

    /* On crée l'objet texture OpenGL puis on lui transfère les pixels. */
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    /* Les mipmaps sont des versions réduites de la texture, utilisées quand
       l'objet est loin de la caméra : c'est plus rapide et moins crénelé. */
    glGenerateMipmap(GL_TEXTURE_2D);

    /* Réglages : la texture se répète au-delà de [0, 1] et est lissée. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Les pixels sont désormais dans la mémoire du GPU : on libère le CPU. */
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);
}

/* Le destructeur rend la texture au GPU pour éviter toute fuite mémoire. */
Texture::~Texture() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
    }
}

/* Déplacement : on prend l'identifiant de l'autre texture et on le remet à 0
   chez elle, pour qu'une seule des deux possède la ressource GPU. */
Texture::Texture(Texture&& other) noexcept : m_id(std::exchange(other.m_id, 0)) {}

/* Affectation par déplacement : on libère d'abord notre éventuelle texture,
   puis on récupère celle de l'autre. */
Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) {
            glDeleteTextures(1, &m_id);
        }
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}

/* Choisit l'unité de texture active puis y attache notre texture. */
void Texture::bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

}  /* namespace artouste::render */
