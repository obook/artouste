/*
 * Fenetre.cpp
 * Implémentation de la fenêtre de détail : allocation de la texture torique,
 * suivi de l'appareil, lecture des tuiles en fil de fond et envoi au GPU (voir
 * Fenetre.hpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/tuiles/Fenetre.hpp"

#include "render/tuiles/FenetreInterne.hpp"

#include "render/Texture.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace artouste::render::tuiles {


Fenetre::Fenetre(Pyramide pyramide, int cotePx) : m_pyramide(std::move(pyramide)) {
    const Calage& calage = m_pyramide.calage();
    if (!calage.valide()) {
        return;
    }
    if (!bc7Disponible()) {
        std::fprintf(stderr, "[tuiles] BC7 indisponible : pas de fenêtre de détail.\n");
        return;
    }

    /* La fenêtre est un nombre entier de tuiles : on rabote plutôt que
       d'accepter un emplacement tronqué, dont les envois partiels ne
       tomberaient plus sur des frontières de blocs BC7. */
    m_nbTuiles = std::max(2, cotePx / calage.tuilePx);
    m_cotePx   = m_nbTuiles * calage.tuilePx;

    m_emplacements.assign(static_cast<std::size_t>(m_nbTuiles) *
                              static_cast<std::size_t>(m_nbTuiles),
                          Emplacement{});
    m_masque.assign(m_emplacements.size(), 0);

    allouerTexture();
    if (m_texture == 0) {
        return;
    }
    allouerResidence();

    m_fil = std::thread(&Fenetre::boucleLecture, this);

    std::printf("[tuiles] fenêtre %d px (%d x %d tuiles de %d px), %.3f m/px, %.0f m au sol.\n",
                m_cotePx,
                m_nbTuiles,
                m_nbTuiles,
                calage.tuilePx,
                static_cast<double>(calage.mParPixel),
                static_cast<double>(tailleM()));
}

Fenetre::~Fenetre() {
    m_fini = true;
    m_signal.notify_all();
    if (m_fil.joinable()) {
        m_fil.join();
    }
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
    if (m_residence != 0) {
        glDeleteTextures(1, &m_residence);
    }
}

void Fenetre::suivre(float x, float z, float dt) {
    if (m_texture == 0) {
        return;
    }
    const Calage& calage = m_pyramide.calage();
    const float   tuileM = calage.tuileM();

    /* Tuile de l'appareil, même si elle tombe hors de la grille : la fenêtre
       reste centrée sur lui, les emplacements hors grille restent simplement
       vides. Le plancher est pris avant la conversion, sans quoi la troncature
       du C++ ramènerait une tuile d'indice négatif (à l'ouest ou au nord de
       l'ancre) sur la tuile 0. */
    const int centreCol    = static_cast<int>(std::floor((x - calage.coinX) / tuileM));
    const int centreRangee = static_cast<int>(std::floor((z - calage.coinZ) / tuileM));

    const int moitie = m_nbTuiles / 2;
    const int col0   = centreCol - moitie;
    const int rang0  = centreRangee - moitie;

    /* Envoi au GPU des tuiles lues depuis la dernière image, dans la limite du
       plafond : le reste attendra l'image suivante. */
    std::vector<Prete> aEnvoyer;
    {
        std::lock_guard<std::mutex> verrou(m_mutex);
        for (int i = 0; i < TUILES_PAR_IMAGE && !m_pretes.empty(); ++i) {
            aEnvoyer.push_back(std::move(m_pretes.front()));
            m_pretes.pop_front();
        }
    }
    for (const Prete& prete : aEnvoyer) {
        envoyer(prete);
    }
    if (!aEnvoyer.empty()) {
        m_signal.notify_all(); /* de la place s'est libérée dans la file */
    }

    /* Revue des emplacements : fondu de ceux qui portent la bonne tuile,
       inventaire des manquantes pour les autres. */
    std::vector<Demande> manquantes;
    m_attendues = 0;
    for (int dr = 0; dr < m_nbTuiles; ++dr) {
        for (int dc = 0; dc < m_nbTuiles; ++dc) {
            const int    col    = col0 + dc;
            const int    rangee = rang0 + dr;
            Emplacement& e      = emplacement(col, rangee);

            if (m_pyramide.dansGrille(col, rangee)) {
                ++m_attendues;
            }
            if (e.col == col && e.rangee == rangee) {
                e.fondu = std::min(1.0f, e.fondu + dt / DUREE_FONDU_S);
                continue;
            }
            /* L'emplacement ne porte pas (encore) la tuile attendue : le shader
               ne doit pas s'en servir. */
            e.fondu = 0.0f;
            /* Hors grille : rien à charger, l'orthophoto d'ensemble suffit.
               Déjà cherchée en vain : ne pas insister. */
            const bool absente = (e.absCol == col && e.absRangee == rangee);
            if (m_pyramide.dansGrille(col, rangee) && !absente) {
                manquantes.push_back(Demande{col, rangee});
            }
        }
    }

    /* La liste des demandes est reconstruite en entier à chaque image et
       remplace la précédente : elle valait pour une position antérieure de la
       fenêtre, et une tuile qu'on aurait retirée de la file en la croyant
       demandée une fois pour toutes ne serait jamais relue. Les demandes
       partent du centre vers les bords : ce qui est sous l'appareil compte plus
       que ce qui est au loin. */
    std::sort(manquantes.begin(),
              manquantes.end(),
              [centreCol, centreRangee](const Demande& a, const Demande& b) {
                  const int da = (a.col - centreCol) * (a.col - centreCol) +
                                 (a.rangee - centreRangee) * (a.rangee - centreRangee);
                  const int db = (b.col - centreCol) * (b.col - centreCol) +
                                 (b.rangee - centreRangee) * (b.rangee - centreRangee);
                  return da < db;
              });

    {
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_demandes.clear();
        for (const Demande& d : manquantes) {
            /* Déjà lue et en attente d'envoi, ou en cours de lecture : la
               redemander ferait relire le même fichier pour rien. */
            const bool enAttente =
                std::any_of(m_pretes.begin(), m_pretes.end(), [&d](const Prete& p) {
                    return p.col == d.col && p.rangee == d.rangee;
                });
            if (enAttente || (m_enLecture.col == d.col && m_enLecture.rangee == d.rangee)) {
                continue;
            }
            m_demandes.push_back(d);
        }
    }
    m_signal.notify_all();

    majResidence();
}

} /* namespace artouste::render::tuiles */
