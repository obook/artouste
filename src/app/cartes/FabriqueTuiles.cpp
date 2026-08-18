/*
 * FabriqueTuiles.cpp
 * Fabrique de tuiles d'orthophoto : cycle de vie du fil de travail.
 *
 * Le calage, l'accès réseau et le fil lui-même sont dans cartes/fabrique/.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/cartes/FabriqueTuiles.hpp"

#include "app/cartes/fabrique/FabriqueInterne.hpp"

#include <filesystem>
#include <mutex>

namespace artouste::app::cartes {

Fabrique::~Fabrique() {
    annuler();
}

bool Fabrique::lancer(const std::filesystem::path& dossierCarte,
                      const std::filesystem::path& dossierSortie,
                      float                        mParPixel) {
    if (m_enCours.load()) {
        return false;
    }
    if (m_fil.joinable()) {
        m_fil.join();  /* fabrication précédente terminée : on récupère son fil */
    }
    m_arret.store(false);
    m_enCours.store(true);
    {
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_avancement = Avancement{};
        m_avancement.message = "Préparation...";
    }
    m_fil = std::thread(&Fabrique::boucle, this, dossierCarte, dossierSortie, mParPixel);
    return true;
}

void Fabrique::annuler() {
    m_arret.store(true);
    if (m_fil.joinable()) {
        m_fil.join();
    }
    m_enCours.store(false);
}

Avancement Fabrique::avancement() const {
    std::lock_guard<std::mutex> verrou(m_mutex);
    return m_avancement;
}

void Fabrique::oublier() {
    if (m_enCours.load()) {
        return;
    }
    std::lock_guard<std::mutex> verrou(m_mutex);
    m_avancement = Avancement{};
}

} /* namespace artouste::app::cartes */
