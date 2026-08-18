/*
 * FenetreEnvoi.cpp
 * Envoi d'une tuile au GPU, mise à jour de la résidence, état de la fenêtre.
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

void Fenetre::envoyer(const Prete& prete) {
    const Calage& calage = m_pyramide.calage();
    Emplacement&  place  = emplacement(prete.col, prete.rangee);

    /* Fichier absent, abîmé, ou tuile d'un autre format que l'index : on note
       l'absence sur l'emplacement pour ne pas relancer la lecture à chaque
       image. L'orthophoto d'ensemble couvre ce carré, il n'y manquera que du
       détail. */
    if (prete.image.niveaux.empty() || prete.image.largeur != calage.tuilePx ||
        prete.image.hauteur != calage.tuilePx) {
        if (!prete.image.niveaux.empty()) {
            std::fprintf(stderr,
                         "[tuiles] tuile (%d, %d) de %d x %d px, attendu %d : ignorée.\n",
                         prete.col,
                         prete.rangee,
                         prete.image.largeur,
                         prete.image.hauteur,
                         calage.tuilePx);
        }
        place.absCol    = prete.col;
        place.absRangee = prete.rangee;
        return;
    }

    const int xBase = modulo(prete.col, m_nbTuiles) * calage.tuilePx;
    const int yBase = modulo(prete.rangee, m_nbTuiles) * calage.tuilePx;

    glBindTexture(GL_TEXTURE_2D, m_texture);
    const int niveaux = std::min<int>(NIVEAUX_FENETRE,
                                      static_cast<int>(prete.image.niveaux.size()));
    for (int i = 0; i < niveaux; ++i) {
        const dds::Niveau& n = prete.image.niveaux[static_cast<std::size_t>(i)];
        if (n.largeur < 4 || n.hauteur < 4) {
            break;  /* sous le bloc BC7 : plus alignable */
        }
        glCompressedTexSubImage2D(GL_TEXTURE_2D,
                                  i,
                                  xBase >> i,
                                  yBase >> i,
                                  n.largeur,
                                  n.hauteur,
                                  GL_COMPRESSED_RGBA_BPTC_UNORM,
                                  static_cast<GLsizei>(n.octets),
                                  prete.image.donnees.data() + n.decalage);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    place.col       = prete.col;
    place.rangee    = prete.rangee;
    place.fondu     = 0.0f;
    place.absCol    = -1;
    place.absRangee = -1;
}

void Fenetre::majResidence() {
    bool change = false;
    for (std::size_t i = 0; i < m_emplacements.size(); ++i) {
        const auto valeur =
            static_cast<unsigned char>(std::clamp(m_emplacements[i].fondu, 0.0f, 1.0f) * 255.0f);
        if (m_masque[i] != valeur) {
            m_masque[i] = valeur;
            change      = true;
        }
    }
    if (!change) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, m_residence);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    m_nbTuiles,
                    m_nbTuiles,
                    GL_RED,
                    GL_UNSIGNED_BYTE,
                    m_masque.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool Fenetre::stabilisee() const noexcept {
    if (m_texture == 0) {
        return true;  /* pas de fenêtre : rien à attendre */
    }
    int regles = 0;
    for (const Emplacement& e : m_emplacements) {
        if ((e.col >= 0 && e.fondu >= 1.0f) || e.absCol >= 0) {
            ++regles;
        }
    }
    return regles >= m_attendues;
}

int Fenetre::residentes() const noexcept {
    int n = 0;
    for (const Emplacement& e : m_emplacements) {
        if (e.col >= 0 && e.fondu >= 1.0f) {
            ++n;
        }
    }
    return n;
}

void Fenetre::bind(unsigned int uniteFenetre, unsigned int uniteResidence) const {
    glActiveTexture(GL_TEXTURE0 + uniteFenetre);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glActiveTexture(GL_TEXTURE0 + uniteResidence);
    glBindTexture(GL_TEXTURE_2D, m_residence);
}

} /* namespace artouste::render::tuiles */
