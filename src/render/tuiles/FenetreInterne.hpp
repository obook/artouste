/*
 * FenetreInterne.hpp
 * Jetons OpenGL et petites aides partagés par les fichiers de la fenêtre de
 * tuiles.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <cstddef>

namespace artouste::render::tuiles {

/* Même jeton qu'ailleurs dans le moteur : BC7 est cœur depuis OpenGL 4.2 et le
   profil glad du projet est figé en 4.1 (voir Texture.cpp). */
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
constexpr unsigned int GL_COMPRESSED_RGBA_BPTC_UNORM = 0x8E8C;
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
constexpr unsigned int GL_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE;
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
constexpr unsigned int GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;
#endif

/* Nombre de tuiles qu'on accepte de garder lues d'avance. Une tuile pèse
   341 Ko : au-delà d'une poignée, le fil de fond travaillerait pour des
   emplacements que la fenêtre a déjà quittés. */
constexpr std::size_t PRETES_MAX = 12;

using artouste::modulo;

} /* namespace artouste::render::tuiles */
