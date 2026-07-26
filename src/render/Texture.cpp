/*
 * Texture.cpp
 * Implémentation de la texture OpenGL : chargement du fichier image avec
 * stb_image, envoi des pixels au GPU et gestion du cycle de vie (déplacement,
 * destruction).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/Texture.hpp"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <utility>

namespace artouste::render {

namespace {

/* GL_EXT_texture_filter_anisotropic (cœur depuis OpenGL 4.6, mais le profil
   glad de ce projet est figé en 4.1 core) : jetons numériques identiques dans
   les deux cas, absents des en-têtes générés. L'extension est prise en charge
   par la quasi-totalité des GPU de bureau depuis longtemps ; glTexParameterf
   ignore silencieusement (GL_INVALID_ENUM) un jeton non reconnu sur un
   pilote qui ne la supporterait vraiment pas, sans planter. */
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
constexpr unsigned int GL_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE;
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
constexpr unsigned int GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;
#endif

/* BC7 (GL_ARB_texture_compression_bptc, cœur depuis OpenGL 4.2 ; le profil glad
   du projet est figé en 4.1 core, d'où le jeton en dur). Choisi plutôt que BC1
   (DXT1, huit fois moins de mémoire au lieu de quatre) parce que BC1 ne code
   que deux couleurs de base par bloc de 4x4 : il salit les aplats très
   contrastés, donc précisément les marquages peints blancs sur bitume des
   aérodromes. BC7 interpole finement et reste indiscernable à l'oeil. */
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
constexpr unsigned int GL_COMPRESSED_RGBA_BPTC_UNORM = 0x8E8C;
#endif
constexpr unsigned int GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ = 0x86A0;

/* Le pilote annonce-t-il BC7 ? Sans lui, glTexImage2D rejetterait le format
   interne et la texture serait perdue : on retombe alors sur du RGBA8. La
   réponse ne change pas d'un appel à l'autre, d'où le calcul unique. */
bool bptcDisponible() {
    static const bool dispo = [] {
        int nb = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &nb);
        for (int i = 0; i < nb; ++i) {
            const auto* nom = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS,
                                                                        static_cast<GLuint>(i)));
            if (nom != nullptr && std::string_view(nom) == "GL_ARB_texture_compression_bptc") {
                return true;
            }
        }
        std::fprintf(stderr, "[Texture] BC7 indisponible sur ce pilote, orthophoto en RGBA8.\n");
        return false;
    }();
    return dispo;
}

}  /* namespace */

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
    upload(pixels, width, height);
    stbi_image_free(pixels);
}

Texture::Texture(const unsigned char* data, std::size_t size) {
    stbi_set_flip_vertically_on_load(1);

    int width    = 0;
    int height   = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height,
                                                  &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        std::fprintf(stderr, "[Texture] échec du décodage (image embarquée).\n");
        return;
    }
    upload(pixels, width, height);
    stbi_image_free(pixels);
}

void Texture::upload(const unsigned char* pixels, int width, int height) {
    m_width  = width;
    m_height = height;

    /* On crée l'objet texture OpenGL puis on lui transfère les pixels. */
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    /* Les mipmaps sont des versions réduites de la texture, utilisées quand
       l'objet est loin de la caméra : c'est plus rapide et moins crénelé. */
    glGenerateMipmap(GL_TEXTURE_2D);

    reglerFiltrage();

    /* Les pixels sont désormais dans la mémoire du GPU : on libère le CPU. */
    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::Texture(const dds::Image& blocsBc7) {
    if (blocsBc7.niveaux.empty() || blocsBc7.donnees.empty() || !bptcDisponible()) {
        return;
    }
    m_width  = blocsBc7.largeur;
    m_height = blocsBc7.hauteur;

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    /* Les mipmaps viennent du cache, on ne les régénère donc pas : c'est
       justement ce qu'on a payé une fois pour toutes. Il faut en revanche dire
       au pilote combien de niveaux existent, sinon il attend la chaîne complète
       et considère la texture incomplète. */
    for (std::size_t i = 0; i < blocsBc7.niveaux.size(); ++i) {
        const dds::Niveau& n = blocsBc7.niveaux[i];
        glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(i),
                               GL_COMPRESSED_RGBA_BPTC_UNORM, n.largeur, n.hauteur, 0,
                               static_cast<GLsizei>(n.octets),
                               blocsBc7.donnees.data() + n.decalage);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                    static_cast<GLint>(blocsBc7.niveaux.size()) - 1);

    reglerFiltrage();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::reglerFiltrage() {
    /* Réglages : la texture se répète au-delà de [0, 1] et est lissée. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Filtrage anisotrope : sans lui, le mipmap isotrope choisit un niveau
       d'après le PIRE axe de déformation de l'UV à l'écran, ce qui sur-réduit
       le sol vu en rasant (piste, hélipad) et le rend pâté par blocs, même si
       la texture source est fine. Flagrant sur l'ortho du terrain à faible
       hauteur. Le pilote borne lui-même la valeur (souvent 16x) via
       GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT ; on la relit pour ne pas dépasser. */
    GLfloat maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAniso, 16.0f));
}

/* Le destructeur rend la texture au GPU pour éviter toute fuite mémoire. */
Texture::~Texture() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
    }
}

/* Déplacement : on prend l'identifiant de l'autre texture et on le remet à 0
   chez elle, pour qu'une seule des deux possède la ressource GPU. */
Texture::Texture(Texture&& other) noexcept
    : m_id(std::exchange(other.m_id, 0)),
      m_width(std::exchange(other.m_width, 0)),
      m_height(std::exchange(other.m_height, 0)) {}

/* Affectation par déplacement : on libère d'abord notre éventuelle texture,
   puis on récupère celle de l'autre. */
Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) {
            glDeleteTextures(1, &m_id);
        }
        m_id     = std::exchange(other.m_id, 0);
        m_width  = std::exchange(other.m_width, 0);
        m_height = std::exchange(other.m_height, 0);
    }
    return *this;
}

/* Choisit l'unité de texture active puis y attache notre texture. */
void Texture::bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

}  /* namespace artouste::render */
