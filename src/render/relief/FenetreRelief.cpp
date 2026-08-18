/*
 * FenetreRelief.cpp
 * Implémentation de la fenêtre de relief fin : lecture de l'index et des tuiles,
 * texture torique d'altitudes, grille dessinée et interrogation du sol (voir
 * FenetreRelief.hpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/relief/FenetreRelief.hpp"

#include "render/relief/FenetreReliefInterne.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace artouste::render::relief {

FenetreRelief::FenetreRelief(std::filesystem::path dossier, const Calage& calage,
                             Correcteur correcteur, int coteGrillePoints)
    : m_dossier(std::move(dossier)), m_calage(calage), m_correcteur(std::move(correcteur)) {
    (void)coteGrillePoints;  /* les grilles sont construites par ouvrir() */
    m_cotePoints = TUILES_FENETRE * m_calage.tuilePoints;
    m_emplacements.assign(static_cast<std::size_t>(TUILES_FENETRE) * TUILES_FENETRE,
                          Emplacement{});
    m_hauteurs.assign(static_cast<std::size_t>(m_cotePoints) *
                          static_cast<std::size_t>(m_cotePoints),
                      0.0f);
}

FenetreRelief::~FenetreRelief() {
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
    for (Grille& grille : m_grilles) {
        if (grille.ebo != 0) {
            glDeleteBuffers(1, &grille.ebo);
        }
        if (grille.vao != 0) {
            glDeleteVertexArrays(1, &grille.vao);
        }
    }
}

void FenetreRelief::bind(unsigned int unite) const {
    glActiveTexture(GL_TEXTURE0 + unite);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glActiveTexture(GL_TEXTURE0);
}

void FenetreRelief::dessiner(int niveau) const {
    const Grille& grille = m_grilles[static_cast<std::size_t>(niveau)];
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(REPRISE);
    glBindVertexArray(grille.vao);
    glDrawElements(GL_TRIANGLE_STRIP, grille.indices, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glDisable(GL_PRIMITIVE_RESTART);
}

float FenetreRelief::distanceDetailM() const noexcept {
    /* ARTOUSTE_DEBUG_RELIEF_D : fixe la distance de pleine finesse, sans quoi
       agrandir le noyau change aussi la loi de finesse. À retirer. */
    static const float force = [] {
        const char* e = std::getenv("ARTOUSTE_DEBUG_RELIEF_D");
        return (e != nullptr) ? std::strtof(e, nullptr) : 0.0f;
    }();
    if (force > 0.0f) {
        return force;
    }
    const Grille& noyau = m_grilles.front();
    const Grille& large = m_grilles.back();
    const float   demiNoyau = 0.5f * static_cast<float>(noyau.cote - 1) * noyau.pasX;
    const float   rapport   = std::max(1.0f, large.pasX / m_calage.pasX);
    /* ÉPINGLÉ à la valeur d'avant le report, le temps de la recette en vol :
       Olivier juge deux composantes de lumière, pas une portée de détail réduite
       de moitié qu'il n'a jamais vue.

       L'autre loi, (demiNoyau - demi-pas de calage) / (2 x rapport), fait lire à
       l'anneau un champ deux fois plus grossier que son pas, ce qui efface la
       marche au bord du noyau, mais divise cette distance par deux. Elle se
       tranchera avec le choix C/B au report. Le journal imprime les deux. */
    return demiNoyau / rapport;
}

float FenetreRelief::niveauLissage() const noexcept {
    /* Un niveau de réduction moyenne deux points par deux : le niveau n lisse
       sur 2^n points. On prend le plus proche de la maille de la carte. */
    /* Un niveau de réduction halve les DEUX axes : une seule valeur doit servir
       aux deux pas. On prend leur moyenne géométrique. */
    const float pas = std::sqrt(m_calage.pasX * m_calage.pasZ);
    return std::max(0.0f, std::round(std::log2(MAILLE_CARTE_M / pas)));
}

bool FenetreRelief::detailEn(float x, float z, float hauteurCarte, float& detail,
                             float& poids) const noexcept {
    /* Tant qu'aucune tuile n'est posée, les altitudes sont nulles : répondre
       ferait naître l'appareil mille mètres sous le sol, le placement du départ
       interrogeant le terrain avant la première image. */
    if (m_texture == 0 || m_premier) {
        return false;
    }

    /* Fondu du bord, MÊME formule que terrain.vert : distance à l'oeil, adoucie
       comme son smoothstep. */
    const float bord  = std::sqrt((x - m_oeilX) * (x - m_oeilX) +
                                 (z - m_oeilZ) * (z - m_oeilZ));
    const float debut = fonduDebutM();
    const float fin   = fonduFinM();
    if (bord >= fin) {
        return false;
    }
    poids = (bord <= debut) ? 1.0f : 1.0f - (bord - debut) / (fin - debut);
    poids = poids * poids * (3.0f - 2.0f * poids);

    const float fi = (x - m_calage.coinX) / m_calage.pasX;
    const float fj = (z - m_calage.coinZ) / m_calage.pasZ;
    const int   i0  = static_cast<int>(std::floor(fi));
    const int   j0  = static_cast<int>(std::floor(fj));
    const float tx  = fi - static_cast<float>(i0);
    const float tz  = fj - static_cast<float>(j0);

    const auto at = [this](int i, int j) {
        return m_hauteurs[static_cast<std::size_t>(modulo(j, m_cotePoints)) *
                              static_cast<std::size_t>(m_cotePoints) +
                          static_cast<std::size_t>(modulo(i, m_cotePoints))];
    };
    /* Le point lu à un niveau de réduction donné, exactement comme la carte
       graphique le calcule : moyenne de blocs de 2^niveau, puis interpolation
       bilinéaire entre les blocs. */
    const auto niveau = [&](int n) {
        const int   cote = 1 << n;
        const float gi   = (fi + 0.5f) / static_cast<float>(cote) - 0.5f;
        const float gj   = (fj + 0.5f) / static_cast<float>(cote) - 0.5f;
        const int   mi   = static_cast<int>(std::floor(gi));
        const int   mj   = static_cast<int>(std::floor(gj));
        const float ux   = gi - static_cast<float>(mi);
        const float uz   = gj - static_cast<float>(mj);

        const auto bloc = [&at, cote](int bi, int bj) {
            float somme = 0.0f;
            for (int j = 0; j < cote; ++j) {
                for (int i = 0; i < cote; ++i) {
                    somme += at(bi * cote + i, bj * cote + j);
                }
            }
            return somme / static_cast<float>(cote * cote);
        };
        const float a = bloc(mi, mj);
        const float b = bloc(mi + 1, mj);
        const float c = bloc(mi, mj + 1);
        const float d = bloc(mi + 1, mj + 1);
        return (a * (1.0f - ux) + b * ux) * (1.0f - uz) + (c * (1.0f - ux) + d * ux) * uz;
    };

    /* Finesse selon la distance, MÊME formule que terrain.vert (finesseDetail) :
       pleine finesse jusqu'à distanceDetailM, puis résolution divisée par deux à
       chaque doublement de distance. */
    const float lissage = niveauLissage();
    const float finesse =
        std::clamp(std::log2(std::max(bord, 1.0f) / distanceDetailM()), 0.0f, lissage);

    /* Le niveau 0 est le cas courant, l'appareil volant près du centre : on lui
       garde le chemin direct, une simple bilinéaire, plutôt que la moyenne de
       blocs qui lui serait équivalente. */
    const int   bas = static_cast<int>(std::floor(finesse));
    const float f   = finesse - static_cast<float>(bas);
    const auto  at0 = [&](int n) {
        return (n == 0) ? (at(i0, j0) * (1.0f - tx) + at(i0 + 1, j0) * tx) * (1.0f - tz) +
                              (at(i0, j0 + 1) * (1.0f - tx) + at(i0 + 1, j0 + 1) * tx) * tz
                        : niveau(n);
    };
    const float lu = (f <= 0.0f) ? at0(bas) : at0(bas) * (1.0f - f) + at0(bas + 1) * f;

    /* L'écart à LA CARTE, pas au laser lissé : MÊME formule que detailFin dans
       terrain.vert, sans quoi l'appareil se pose ailleurs que sur ce qu'il voit. */
    detail = lu - hauteurCarte;
    return true;
}

} /* namespace artouste::render::relief */
