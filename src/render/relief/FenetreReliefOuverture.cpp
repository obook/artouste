/*
 * FenetreReliefOuverture.cpp
 * Ouverture d'un jeu de tuiles de relief : index, calage, grilles.
 *
 * Auteur : O. Booklage
 * Date : août 2026
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

std::unique_ptr<FenetreRelief> FenetreRelief::ouvrir(const std::filesystem::path& dossier,
                                                     Correcteur                   correcteur,
                                                     int coteGrillePoints) {
    std::ifstream in(dossier / NOM_INDEX);
    if (!in) {
        return nullptr;
    }

    /* Même convention que l'index des tuiles d'image : une clé par ligne, # en
       commentaire, clé inconnue ignorée. */
    Calage      calage;
    std::string cle;
    while (in >> cle) {
        if (!cle.empty() && cle[0] == '#') {
            std::getline(in, cle);
            continue;
        }
        if (cle == "tuile_points") {
            in >> calage.tuilePoints;
        } else if (cle == "pas_m") {
            /* Jeu de tuiles v1 : un seul pas, isotrope. */
            in >> calage.pasX;
            calage.pasZ = calage.pasX;
        } else if (cle == "pas_x") {
            in >> calage.pasX;
        } else if (cle == "pas_z") {
            in >> calage.pasZ;
        } else if (cle == "colonnes") {
            in >> calage.colonnes;
        } else if (cle == "rangees") {
            in >> calage.rangees;
        } else if (cle == "coin_x") {
            in >> calage.coinX;
        } else if (cle == "coin_z") {
            in >> calage.coinZ;
        } else {
            std::getline(in, cle);
        }
    }

    if (!calage.valide() || !correcteur) {
        return nullptr;
    }

    std::unique_ptr<FenetreRelief> fenetre(
        new FenetreRelief(dossier, calage, std::move(correcteur), coteGrillePoints));
    if (!fenetre->allouerTexture()) {
        return nullptr;
    }

    /* Noyau au pas des tuiles, puis anneau quatre fois plus grossier et quatre
       fois plus large. L'anneau est écarté s'il déborde des tuiles que le tore
       garantit présentes. */
    /* La tuile n'étant pas carrée au sol, la marge garantie est celle du plus
       PETIT côté : c'est lui qui limite ce que le tore tient à coup sûr. */
    const float marge = static_cast<float>(TUILES_FENETRE / 2 - 1) *
                        std::min(calage.tuileX(), calage.tuileZ());
    const int   coteNoyau = std::clamp(coteGrillePoints, 64, fenetre->m_cotePoints / 2);
    if (!fenetre->construireGrille(coteNoyau, calage.pasX, calage.pasZ)) {
        return nullptr;
    }
    /* ARTOUSTE_DEBUG_RELIEF_ANNEAU="cote pas" : côté et pas de l'anneau à
       l'exécution, pour comparer deux anneaux avec UN SEUL binaire. Le garder
       impair si son pas vaut celui des texels. À retirer. */
    int coteAnneau = COTE_ANNEAU_POINTS;
    int multAnneau = PAS_ANNEAU;
    if (const char* e = std::getenv("ARTOUSTE_DEBUG_RELIEF_ANNEAU")) {
        std::sscanf(e, "%d %d", &coteAnneau, &multAnneau);
    }
    const float pasAnneauX = static_cast<float>(multAnneau) * calage.pasX;
    const float pasAnneauZ = static_cast<float>(multAnneau) * calage.pasZ;
    const float pasAnneau  = std::max(pasAnneauX, pasAnneauZ);
    /* L'anneau est RABOTÉ pour tenir dans la marge que le tore garantit, au lieu
       d'être abandonné : sans cela un pas de tuile un peu plus grand supprime
       l'anneau entier en silence, et la fenêtre perd les trois quarts de son
       emprise. Le côté reste impair, comme celui du noyau. */
    const int coteMarge = 1 + 2 * static_cast<int>(marge / pasAnneau);
    coteAnneau          = std::min(coteAnneau, coteMarge | 1);
    /* Il ne vaut la peine que s'il DÉBORDE le noyau : on compare des emprises au
       sol, pas des nombres de points, l'anneau ayant moins de points sur plus de
       terrain. */
    const float demiAnneau = 0.5f * static_cast<float>(coteAnneau - 1) * pasAnneau;
    const float demiNoyau  = 0.5f * static_cast<float>(coteNoyau - 1) *
                            std::max(calage.pasX, calage.pasZ);
    if (demiAnneau > demiNoyau) {
        (void)fenetre->construireGrille(coteAnneau, pasAnneauX, pasAnneauZ);
    } else {
        std::printf("[relief] anneau écarté : la marge du tore (%.0f m) ne tient pas "
                    "au-delà du noyau.\n", static_cast<double>(marge));
    }

    for (const Grille& grille : fenetre->m_grilles) {
        std::printf("[relief] grille %d points à %.4f x %.4f m : %.0f x %.0f m au sol, "
                    "%.2f M sommets.\n",
                    grille.cote,
                    static_cast<double>(grille.pasX), static_cast<double>(grille.pasZ),
                    static_cast<double>(grille.cote - 1) * static_cast<double>(grille.pasX),
                    static_cast<double>(grille.cote - 1) * static_cast<double>(grille.pasZ),
                    static_cast<double>(grille.cote) * static_cast<double>(grille.cote) / 1e6);
    }
    {
        const Grille& grilleNoyau = fenetre->m_grilles.front();
        const Grille& large       = fenetre->m_grilles.back();
        const float   demiNoyauX =
            0.5f * static_cast<float>(grilleNoyau.cote - 1) * grilleNoyau.pasX;
        const float   rapport   = std::max(1.0f, large.pasX / calage.pasX);
        std::printf("[relief] fondu du bord : plein jusqu'à %.0f m, éteint à %.0f m.\n",
                    static_cast<double>(fenetre->fonduDebutM()),
                    static_cast<double>(fenetre->fonduFinM()));
        std::printf("[relief] finesse pleine jusqu'à %.1f m, ÉPINGLÉE (loi 2x : %.1f m).\n",
                    static_cast<double>(fenetre->distanceDetailM()),
                    static_cast<double>((demiNoyauX - 0.5f * large.pasX) / (2.0f * rapport)));
    }
    std::printf("[relief] fenêtre %d x %d points à %.4f x %.4f m, %.0f x %.0f m au sol, "
                "%.0f Mo.\n",
                fenetre->m_cotePoints,
                fenetre->m_cotePoints,
                static_cast<double>(calage.pasX),
                static_cast<double>(calage.pasZ),
                static_cast<double>(fenetre->tailleM()),
                static_cast<double>(fenetre->tailleZ()),
                static_cast<double>(fenetre->m_hauteurs.size()) * sizeof(float) / 1e6);
    return fenetre;
}

std::filesystem::path cheminJeuDeRelief(const std::filesystem::path& dossierCarte,
                                        const std::filesystem::path& racine) {
    const std::string nom = dossierCarte.filename().string() + ".relief";

    std::vector<std::filesystem::path> candidats;
    if (const char* env = std::getenv("ARTOUSTE_TUILES"); env != nullptr && env[0] != '\0') {
        candidats.push_back(std::filesystem::path(env) / nom);
    }
    if (!racine.empty()) {
        candidats.push_back(racine / nom);
    }
    candidats.push_back(dossierCarte.parent_path() / nom);
    candidats.push_back(dossierCarte / "relief");

    std::error_code ec;
    for (const std::filesystem::path& candidat : candidats) {
        if (std::filesystem::exists(candidat / NOM_INDEX, ec)) {
            return candidat;
        }
    }
    return {};
}

} /* namespace artouste::render::relief */
