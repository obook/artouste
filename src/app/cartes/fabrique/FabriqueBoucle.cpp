/*
 * FabriqueBoucle.cpp
 * Le fil de fabrication : bloc par bloc, découpage, compression, écriture.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/cartes/FabriqueTuiles.hpp"

#include "app/cartes/fabrique/FabriqueInterne.hpp"

#include "render/Bc7.hpp"
#include "render/Dds.hpp"
#include "render/tuiles/Pyramide.hpp"

#include <stb_image.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace artouste::app::cartes {

void Fabrique::boucle(std::filesystem::path dossierCarte,
                      std::filesystem::path dossierSortie,
                      float                 mParPixel) {
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
    (void)mParPixel;
    finir(true, "Compilé sans libcurl : rien à faire ici.");
#else
    const CalageCarte carte = lireCalage(dossierCarte);
    if (!carte.valide) {
        finir(true, "Calage de carte illisible.");
        return;
    }

    const render::tuiles::Calage calage = grille(carte, mParPixel);
    if (!calage.valide()) {
        finir(true, "Grille de tuiles inutilisable.");
        return;
    }

    /* Un jeu déjà là, mais à une AUTRE finesse, ne peut pas être complété : ses
       fichiers portent les mêmes noms et couvrent d'autres emprises, si bien que
       la reprise, qui saute les tuiles présentes, mélangerait deux échelles sur
       la même carte. On l'efface donc avant d'écrire le nouvel index. L'écran
       l'annonce dans sa confirmation, la règle de cet écran étant de ne rien
       détruire sans le dire (voir ApplicationMenuCartes.cpp). */
    if (const auto ancien = render::tuiles::Pyramide::ouvrir(dossierSortie)) {
        if (std::fabs(ancien->calage().mParPixel - mParPixel) > 1e-4f) {
            std::error_code ec;
            std::filesystem::remove_all(dossierSortie, ec);
        }
    }

    /* L'index d'abord : c'est lui qui fixe la grille, et le moteur s'y réfère.
       Écrit avant la première tuile, pour qu'une fabrication interrompue laisse
       un jeu cohérent, seulement incomplet. */
    const render::tuiles::Pyramide pyramide{dossierSortie, calage};
    if (!pyramide.ecrireIndex()) {
        finir(true, "Impossible d'écrire l'index dans " + dossierSortie.string());
        return;
    }

    /* Témoin d'inachèvement, posé avec l'index et retiré au tout dernier moment.
       C'est lui, et non l'index, qui dit à l'écran des cartes que le jeu est
       partiel : l'index décrit la grille VOULUE, pas celle qui est sur le disque. */
    {
        std::ofstream marqueur(dossierSortie / NOM_MARQUEUR_INACHEVE, std::ios::trunc);
        marqueur << "# Fabrication en cours ou interrompue.\n";
        marqueur << "# Ce fichier disparaît quand le jeu de tuiles est complet.\n";
        marqueur << "# Relancer la fabrication reprend où elle s'est arrêtée.\n";
        marqueur << "m_par_pixel " << mParPixel << "\n";
        marqueur << "tuiles_attendues " << calage.colonnes * calage.rangees << "\n";
    }

    const int blocsX = (calage.colonnes + TUILES_PAR_BLOC - 1) / TUILES_PAR_BLOC;
    const int blocsY = (calage.rangees + TUILES_PAR_BLOC - 1) / TUILES_PAR_BLOC;
    {
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_avancement.blocsTotal = blocsX * blocsY;
        m_avancement.message    = "Téléchargement...";
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        finir(true, "Initialisation réseau impossible.");
        return;
    }

    const double degParMLon = static_cast<double>(carte.lonMax - carte.lonMin) /
                              static_cast<double>(carte.largeurM);
    const double degParMLat = static_cast<double>(carte.latMax - carte.latMin) /
                              static_cast<double>(carte.hauteurM);
    const double tuileM = static_cast<double>(TUILE_PX) * static_cast<double>(mParPixel);

    const auto debut = std::chrono::steady_clock::now();
    std::vector<unsigned char> jpeg;
    std::vector<unsigned char> tuile(static_cast<std::size_t>(TUILE_PX) *
                                     static_cast<std::size_t>(TUILE_PX) * 4);
    std::uintmax_t octetsRecus = 0, octetsEcrits = 0;
    int            tuilesEcrites = 0;
    bool           erreur = false;

    for (int by = 0; by < blocsY && !m_arret.load() && !erreur; ++by) {
        for (int bx = 0; bx < blocsX && !m_arret.load() && !erreur; ++bx) {
            const int col0    = bx * TUILES_PAR_BLOC;
            const int rangee0 = by * TUILES_PAR_BLOC;
            const int nCol    = std::min(TUILES_PAR_BLOC, calage.colonnes - col0);
            const int nRangee = std::min(TUILES_PAR_BLOC, calage.rangees - rangee0);

            /* Emprise du bloc. La grille est ancrée sur le coin nord-ouest, et la
               rangée 0 est au nord : la latitude décroît quand la rangée croît. */
            const double lonLo = static_cast<double>(carte.lonMin) +
                                 static_cast<double>(col0) * tuileM * degParMLon;
            const double lonHi = lonLo + static_cast<double>(nCol) * tuileM * degParMLon;
            const double latHi = static_cast<double>(carte.latMax) -
                                 static_cast<double>(rangee0) * tuileM * degParMLat;
            const double latLo = latHi - static_cast<double>(nRangee) * tuileM * degParMLat;

            bool recu = false;
            for (int essai = 0; essai < 3 && !recu && !m_arret.load(); ++essai) {
                recu = demanderBloc(curl, latLo, lonLo, latHi, lonHi, nCol * TUILE_PX,
                                    nRangee * TUILE_PX, jpeg);
                if (!recu) {
                    std::this_thread::sleep_for(std::chrono::seconds(2 * (essai + 1)));
                }
            }
            if (m_arret.load()) {
                break;
            }
            if (!recu) {
                erreur = true;
                break;
            }
            octetsRecus += jpeg.size();

            /* Rangée 0 au nord, comme l'écrivent les outils de préparation : on ne
               retourne pas l'image, contrairement au chemin de rendu. */
            stbi_set_flip_vertically_on_load(0);
            int            largeur = 0, hauteur = 0, canaux = 0;
            unsigned char* pixels = stbi_load_from_memory(jpeg.data(),
                                                          static_cast<int>(jpeg.size()), &largeur,
                                                          &hauteur, &canaux, 4);
            if (pixels == nullptr || largeur != nCol * TUILE_PX || hauteur != nRangee * TUILE_PX) {
                if (pixels != nullptr) {
                    stbi_image_free(pixels);
                }
                erreur = true;
                break;
            }

            for (int dr = 0; dr < nRangee && !m_arret.load() && !erreur; ++dr) {
                for (int dc = 0; dc < nCol && !m_arret.load() && !erreur; ++dc) {
                    const std::filesystem::path chemin =
                        pyramide.fichier(col0 + dc, rangee0 + dr);
                    if (std::filesystem::exists(chemin)) {
                        continue;  /* reprise : cette tuile est déjà là */
                    }
                    /* Le bloc est à la finesse cible et aligné sur la grille : une
                       tuile y est une simple recopie, sans rééchantillonnage. */
                    for (int y = 0; y < TUILE_PX; ++y) {
                        const std::size_t source =
                            (static_cast<std::size_t>(dr * TUILE_PX + y) *
                                 static_cast<std::size_t>(largeur) +
                             static_cast<std::size_t>(dc * TUILE_PX)) * 4;
                        std::copy_n(pixels + source, static_cast<std::size_t>(TUILE_PX) * 4,
                                    tuile.begin() + static_cast<std::ptrdiff_t>(y) * TUILE_PX * 4);
                    }
                    if (partBlanche(tuile) > 0.9f) {
                        continue;  /* hors couverture : à l'orthophoto d'ensemble */
                    }
                    const auto blocs =
                        render::bc7::compresser(tuile.data(), TUILE_PX, TUILE_PX, {});
                    if (!blocs.has_value() ||
                        !render::dds::ecrire(chemin, *blocs, render::dds::Empreinte{})) {
                        erreur = true;
                        break;
                    }
                    std::error_code ec;
                    octetsEcrits += std::filesystem::file_size(chemin, ec);
                    ++tuilesEcrites;
                }
            }
            stbi_image_free(pixels);

            const double secondes =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - debut).count();
            const int faits = by * blocsX + bx + 1;
            std::lock_guard<std::mutex> verrou(m_mutex);
            m_avancement.blocsFaits    = faits;
            m_avancement.tuilesEcrites = tuilesEcrites;
            m_avancement.octetsRecus   = octetsRecus;
            m_avancement.octetsEcrits  = octetsEcrits;
            /* Débit et durée restante déduits de ce qui a RÉELLEMENT été reçu,
               jamais d'une vitesse supposée. */
            if (secondes > 0.5 && faits > 0) {
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
        std::error_code ec;
        std::filesystem::remove(dossierSortie / NOM_MARQUEUR_INACHEVE, ec);
        finir(false, "Terminé : " + std::to_string(tuilesEcrites) + " tuiles, " +
                         formaterOctets(octetsEcrits) + " sur le disque.");
    }
#endif
}

} /* namespace artouste::app::cartes */
