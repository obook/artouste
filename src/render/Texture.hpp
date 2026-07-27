/*
 * Texture.hpp
 * Texture 2D OpenGL chargée depuis un fichier image (PNG via stb_image).
 * La classe possède l'identifiant GL et le libère automatiquement : elle ne
 * peut donc pas être copiée, seulement déplacée (move-only).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <cstddef>
#include <filesystem>

#include "render/Dds.hpp"

namespace artouste::render {

/* Le pilote annonce-t-il BC7 (GL_ARB_texture_compression_bptc) ? Sans lui, un
   format interne compressé est rejeté et la texture est perdue : les appelants
   retombent alors sur du RGBA8 (orthophoto d'ensemble) ou renoncent au détail
   (fenêtre de tuiles). La réponse ne change pas d'un appel à l'autre, elle est
   donc calculée une seule fois. Demande un contexte OpenGL courant. */
[[nodiscard]] bool bc7Disponible();

class Texture {
public:
    Texture() = default;
    explicit Texture(const std::filesystem::path& path);

    /* Construit la texture à partir de blocs BC7 déjà compressés, mipmaps
       compris (voir TextureCache.hpp). Quatre fois moins de mémoire vidéo que
       le RGBA8 pour une perte visuelle indiscernable, sans le coût de
       compression : les blocs sont envoyés tels quels. */
    explicit Texture(const dds::Image& blocsBc7);

    /* Décode une image (PNG/JPG) depuis un tampon mémoire au lieu d'un fichier :
       sert aux textures embarquées dans un modèle glTF/.glb (voir
       ModelLoader::resolveTexture, cas d'une texture référencée "*N" plutôt
       qu'un chemin de fichier). */
    Texture(const unsigned char* data, std::size_t size);

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

    /* Dimensions en pixels de l'image envoyée au GPU. Terrain s'en sert pour
       connaître la finesse réelle de l'orthophoto (mètres par pixel), dont
       dépend le dosage du grain de détail (voir terrain.frag). */
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }

private:
    /* Envoie des pixels RGBA déjà décodés vers le GPU (commun aux deux
       constructeurs, fichier ou mémoire). */
    void upload(const unsigned char* pixels, int width, int height);

    /* Réglages communs à tous les chemins : répétition, filtrage trilinéaire
       et anisotrope. Appelée texture liée. */
    void reglerFiltrage();

    unsigned int m_id     = 0;
    int          m_width  = 0;
    int          m_height = 0;
};

}  /* namespace artouste::render */
