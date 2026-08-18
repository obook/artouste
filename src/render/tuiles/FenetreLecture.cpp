/*
 * FenetreLecture.cpp
 * Le fil qui lit les tuiles du disque, pendant que le rendu continue.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/tuiles/Fenetre.hpp"

#include "render/tuiles/FenetreInterne.hpp"

#include <mutex>
#include <utility>

namespace artouste::render::tuiles {

void Fenetre::boucleLecture() {
    while (!m_fini) {
        Demande demande;
        {
            std::unique_lock<std::mutex> verrou(m_mutex);
            m_signal.wait(verrou, [this] {
                return m_fini || (!m_demandes.empty() && m_pretes.size() < PRETES_MAX);
            });
            if (m_fini) {
                return;
            }
            demande      = m_demandes.front();
            m_enLecture  = demande;  /* pour que le fil de rendu ne la redemande pas */
            m_demandes.pop_front();
        }

        /* Aucun contrôle d'empreinte : une tuile est livrée avec la carte, elle
           n'est pas dérivée d'une source locale (voir Dds.hpp). */
        auto image = dds::lire(m_pyramide.fichier(demande.col, demande.rangee));

        /* Une lecture qui échoue est signalée comme les autres, avec une image
           vide : c'est le fil de rendu qui note l'absence, faute de quoi la
           tuile serait redemandée à chaque image et un jeu de tuiles incomplet
           mitraillerait le disque. */
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_enLecture = Demande{-1, -1};
        m_pretes.push_back(Prete{demande.col,
                                 demande.rangee,
                                 image.has_value() ? std::move(*image) : dds::Image{}});
    }
}

} /* namespace artouste::render::tuiles */
