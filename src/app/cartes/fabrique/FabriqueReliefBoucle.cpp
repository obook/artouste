/*
 * FabriqueReliefBoucle.cpp
 * Le fil de fabrication du relief : index, témoin, puis les blocs un à un.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/cartes/FabriqueRelief.hpp"

#include "app/cartes/fabrique/FabriqueReliefInterne.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>

namespace artouste::app::cartes {

void Fabrique::boucleRelief(std::filesystem::path dossierCarte,
                            std::filesystem::path dossierSortie) {
    const auto finir = [this](bool echec, const std::string& message) {
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_avancement.termine = true;
        m_avancement.echec   = echec;
        m_avancement.message = message;
        m_enCours.store(false);
    };

#ifndef ARTOUSTE_HAS_CURL
    (void)dossierCarte;
    (void)dossierSortie;
    finir(true, "Compilé sans libcurl : rien à faire ici.");
#else
    const CalageCarte  carte  = lireCalage(dossierCarte);
    const GrilleRelief grille = grilleRelief(dossierCarte);
    if (!carte.valide || !grille.valide) {
        finir(true, "Calage ou maillage de la carte illisible.");
        return;
    }
    ReliefCarte ensemble;
    if (!ensemble.charger(dossierCarte, carte)) {
        finir(true, "Relief d'ensemble illisible : les trous du LiDAR ne pourraient pas "
                    "être bouchés.");
        return;
    }

    /* Un jeu déjà là, mais à un AUTRE pas, ne peut pas être complété : ses
       fichiers portent les mêmes noms et couvrent d'autres emprises, si bien que
       la reprise mélangerait deux grilles sur la même carte. On l'efface donc
       avant d'écrire le nouvel index ; l'écran l'annonce dans sa confirmation. */
    const GrilleRelief ancien = lireIndexRelief(dossierSortie);
    std::error_code    ec;
    if (ancien.valide && (std::fabs(ancien.pasX - grille.pasX) > 1e-3f ||
                          std::fabs(ancien.pasZ - grille.pasZ) > 1e-3f)) {
        std::filesystem::remove_all(dossierSortie, ec);
    }
    std::filesystem::create_directories(dossierSortie, ec);

    /* L'index d'abord : c'est lui qui fixe la grille, et le moteur s'y réfère.
       Écrit avant la première tuile, pour qu'une fabrication interrompue laisse
       un jeu cohérent, seulement incomplet. */
    if (!ecrireIndexRelief(dossierSortie, dossierCarte.filename().string(), carte, grille)) {
        finir(true, "Impossible d'écrire l'index dans " + dossierSortie.string());
        return;
    }
    /* Témoin d'inachèvement, dans les mêmes termes que celui des tuiles d'image
       et que celui des scripts : l'index décrit la grille VOULUE, pas celle qui
       est sur le disque. */
    {
        /* Le résumé décrit un jeu complet : il ne doit pas survivre au démarrage
           d'une fabrication, sinon l'écran des cartes garderait l'ancien poids. */
        std::filesystem::remove(dossierSortie / NOM_RESUME, ec);
        std::ofstream marqueur(dossierSortie / NOM_MARQUEUR_INACHEVE, std::ios::trunc);
        marqueur << "# Fabrication en cours ou interrompue.\n";
        marqueur << "# Ce fichier disparaît quand le jeu de tuiles est complet.\n";
        marqueur << "# Relancer la fabrication reprend où elle s'est arrêtée.\n";
        marqueur << "m_par_pixel " << grille.pasX << "\n";
        marqueur << "tuiles_attendues " << grille.tuiles() << "\n";
    }

    const int blocsX = (grille.colonnes + RELIEF_TUILES_PAR_BLOC - 1) / RELIEF_TUILES_PAR_BLOC;
    const int blocsY = (grille.rangees + RELIEF_TUILES_PAR_BLOC - 1) / RELIEF_TUILES_PAR_BLOC;
    {
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_avancement.blocsTotal = blocsX * blocsY;
        m_avancement.message    = "Téléchargement du relief...";
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        finir(true, "Initialisation réseau impossible.");
        return;
    }

    const auto     debut         = std::chrono::steady_clock::now();
    std::uintmax_t octetsRecus   = 0;
    std::uintmax_t octetsEcrits  = 0;
    int            tuilesEcrites = 0;
    bool           erreur        = false;

    for (int by = 0; by < blocsY && !m_arret.load() && !erreur; ++by) {
        for (int bx = 0; bx < blocsX && !m_arret.load() && !erreur; ++bx) {
            const int col0    = bx * RELIEF_TUILES_PAR_BLOC;
            const int rangee0 = by * RELIEF_TUILES_PAR_BLOC;
            const int nCol    = std::min(RELIEF_TUILES_PAR_BLOC, grille.colonnes - col0);
            const int nRangee = std::min(RELIEF_TUILES_PAR_BLOC, grille.rangees - rangee0);

            /* Reprise : ce bloc a déjà été traité, tuiles écrites ou non. */
            if (!std::filesystem::exists(cheminMarqueBloc(dossierSortie, col0, rangee0), ec)) {
                const int ecrites =
                    traiterBlocRelief(curl, carte, grille, ensemble, dossierSortie, col0, rangee0,
                                      nCol, nRangee, m_arret, octetsRecus, octetsEcrits);
                if (ecrites < 0) {
                    erreur = true;
                    break;
                }
                tuilesEcrites += ecrites;
            }
            if (m_arret.load()) {
                break;
            }

            const double secondes =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - debut).count();
            const int                   faits = by * blocsX + bx + 1;
            std::lock_guard<std::mutex> verrou(m_mutex);
            m_avancement.blocsFaits    = faits;
            m_avancement.tuilesEcrites = tuilesEcrites;
            m_avancement.octetsRecus   = octetsRecus;
            m_avancement.octetsEcrits  = octetsEcrits;
            /* Débit et durée restante déduits de ce qui a RÉELLEMENT été reçu,
               jamais d'une vitesse supposée. */
            if (secondes > 0.5 && octetsRecus > 0) {
                m_avancement.octetsParSeconde = static_cast<double>(octetsRecus) / secondes;
                m_avancement.secondesRestantes =
                    secondes / static_cast<double>(faits) *
                    static_cast<double>(m_avancement.blocsTotal - faits);
            }
        }
    }

    curl_easy_cleanup(curl);

    if (erreur) {
        finir(true, "Échec réseau ou données inattendues. Ce qui est écrit est conservé : "
                    "relancer reprendra où on en est.");
    } else if (m_arret.load()) {
        finir(false, "Arrêté. Ce qui est écrit est conservé : relancer reprendra où on en est.");
    } else {
        /* Seul chemin qui retire le témoin : la grille a été parcourue en entier,
           sans arrêt ni erreur. */
        std::filesystem::remove(dossierSortie / NOM_MARQUEUR_INACHEVE, ec);
        ecrireResume(dossierSortie);
        finir(false, "Terminé : " + std::to_string(tuilesEcrites) + " tuiles de relief, " +
                         formaterOctets(octetsEcrits) + " sur le disque.");
    }
#endif
}

} /* namespace artouste::app::cartes */
