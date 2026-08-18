/*
 * FenetreTextures.cpp
 * Texture torique, carte de résidence, et rayons de la fenêtre.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/tuiles/Fenetre.hpp"

#include "render/tuiles/FenetreInterne.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <utility>
#include <vector>

namespace artouste::render::tuiles {

void Fenetre::allouerTexture() {
    /* OpenGL 4.1 n'a pas glTexStorage2D : on réserve chaque niveau par un envoi
       de blocs vides. Un bloc BC7 nul ne décode rien de bon, mais aucun
       fragment ne le lira jamais : la texture de résidence tient un emplacement
       à zéro tant que sa tuile n'est pas arrivée. */
    const int          tuilePx = m_pyramide.calage().tuilePx;
    const std::size_t  octets0 = dds::octetsBc7(m_cotePx, m_cotePx);
    std::vector<unsigned char> vides(octets0, 0);

    /* On part d'une pile d'erreurs vide, sinon une erreur laissée par un appel
       antérieur nous ferait renoncer à tort à la fenêtre. */
    while (glGetError() != GL_NO_ERROR) {
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    int niveaux = 0;
    for (int i = 0; i < NIVEAUX_FENETRE; ++i) {
        const int cote = m_cotePx >> i;
        /* On s'arrête si un emplacement descendrait sous un bloc BC7 : ses
           envois partiels ne seraient plus alignés. */
        if ((tuilePx >> i) < 4) {
            break;
        }
        glCompressedTexImage2D(GL_TEXTURE_2D,
                               i,
                               GL_COMPRESSED_RGBA_BPTC_UNORM,
                               cote,
                               cote,
                               0,
                               static_cast<GLsizei>(dds::octetsBc7(cote, cote)),
                               vides.data());
        ++niveaux;
    }
    if (glGetError() != GL_NO_ERROR) {
        std::fprintf(stderr,
                     "[tuiles] mémoire vidéo refusée pour une fenêtre de %d px : "
                     "pas de fenêtre de détail.\n",
                     m_cotePx);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
        return;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, niveaux - 1);
    /* Répétition : c'est elle qui referme le tore. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Filtrage anisotrope, comme pour l'orthophoto d'ensemble : sans lui, le sol
       vu en rasant est sur-réduit et redevient pâté, ce qui annulerait tout le
       bénéfice du détail là où il compte le plus, juste devant l'appareil (voir
       Texture::reglerFiltrage). */
    GLfloat maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAniso, 16.0f));

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Fenetre::allouerResidence() {
    /* Un octet par emplacement, lu au plus proche : le shader veut l'état de
       SON emplacement, pas une moyenne avec les voisins, qui étalerait le
       détail sur une tuile absente. */
    glGenTextures(1, &m_residence);
    glBindTexture(GL_TEXTURE_2D, m_residence);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_R8,
                 m_nbTuiles,
                 m_nbTuiles,
                 0,
                 GL_RED,
                 GL_UNSIGNED_BYTE,
                 m_masque.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Fenetre::Emplacement& Fenetre::emplacement(int col, int rangee) noexcept {
    const int i = modulo(rangee, m_nbTuiles) * m_nbTuiles + modulo(col, m_nbTuiles);
    return m_emplacements[static_cast<std::size_t>(i)];
}

const Fenetre::Emplacement& Fenetre::emplacement(int col, int rangee) const noexcept {
    const int i = modulo(rangee, m_nbTuiles) * m_nbTuiles + modulo(col, m_nbTuiles);
    return m_emplacements[static_cast<std::size_t>(i)];
}

float Fenetre::rayonPleinM() const noexcept {
    return 0.62f * rayonFonduM();
}

float Fenetre::rayonFonduM() const noexcept {
    /* La fenêtre est centrée sur la TUILE de l'appareil, pas sur l'appareil :
       la marge garantie de chaque côté est donc d'un demi-côté moins une tuile.
       Au-delà, la présence de la tuile n'est pas assurée et le détail doit déjà
       avoir laissé la place à l'orthophoto d'ensemble. */
    const float tuileM = m_pyramide.calage().tuileM();
    return (static_cast<float>(m_nbTuiles) / 2.0f - 1.0f) * tuileM;
}

} /* namespace artouste::render::tuiles */
