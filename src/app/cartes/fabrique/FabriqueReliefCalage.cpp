/*
 * FabriqueReliefCalage.cpp
 * Pas, grille, index et estimation d'une fabrication de relief.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/cartes/FabriqueRelief.hpp"

#include "app/cartes/fabrique/FabriqueReliefInterne.hpp"

#include "render/relief/FenetreRelief.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

namespace artouste::app::cartes {

namespace {

/* Le pas qui s'approche le plus du pas visé PARMI ceux qui s'emboîtent dans la
   maille de la carte. Emboîter veut dire maille / (pas x PAS_ANNEAU) entier :
   il le faut pour le noyau de la fenêtre ET pour son anneau, qui dessine à
   PAS_ANNEAU fois ce pas. Un pas quelconque fait redessiner la surface du
   maillage au lieu de la reproduire, et les silhouettes se déplacent à la
   frontière de la fenêtre, ce qui se voit en vol.

   Les deux axes d'une carte n'ayant pas la même maille, ce calcul est fait par
   axe : une tuile n'est pas carrée au sol. */
[[nodiscard]] float pasEmboite(float maille) {
    if (!(maille > 0.0f)) {
        return 0.0f;
    }
    const float multiple = static_cast<float>(render::relief::PAS_ANNEAU);
    const float ideal    = maille / (RELIEF_PAS_VISE_M * multiple);
    const int   bas      = std::max(1, static_cast<int>(std::floor(ideal)));
    const float pasBas   = maille / (multiple * static_cast<float>(bas));
    const float pasHaut  = maille / (multiple * static_cast<float>(bas + 1));
    return (std::fabs(pasBas - RELIEF_PAS_VISE_M) <= std::fabs(pasHaut - RELIEF_PAS_VISE_M))
               ? pasBas
               : pasHaut;
}

} /* namespace */

GrilleRelief grilleRelief(const std::filesystem::path& dossierCarte) {
    const CalageCarte carte = lireCalage(dossierCarte);
    GrilleRelief      g;
    if (!carte.valide || carte.mailleColonnes < 2 || carte.mailleRangees < 2) {
        return g;
    }
    g.pasX = pasEmboite(carte.largeurM / static_cast<float>(carte.mailleColonnes - 1));
    g.pasZ = pasEmboite(carte.hauteurM / static_cast<float>(carte.mailleRangees - 1));
    if (g.pasX <= 0.0f || g.pasZ <= 0.0f) {
        return g;
    }
    const float tuileX = static_cast<float>(RELIEF_TUILE_POINTS) * g.pasX;
    const float tuileZ = static_cast<float>(RELIEF_TUILE_POINTS) * g.pasZ;
    g.colonnes = static_cast<int>(std::ceil(carte.largeurM / tuileX));
    g.rangees  = static_cast<int>(std::ceil(carte.hauteurM / tuileZ));
    g.valide   = g.colonnes > 0 && g.rangees > 0;
    return g;
}

GrilleRelief lireIndexRelief(const std::filesystem::path& dossierRelief) {
    GrilleRelief g;
    if (dossierRelief.empty()) {
        return g;
    }
    std::ifstream in(dossierRelief / render::relief::NOM_INDEX);
    if (!in) {
        return g;
    }
    /* Mêmes clés que l'ouverture du jeu par le moteur (FenetreReliefOuverture) :
       une clé par ligne, # en commentaire, clé inconnue ignorée. */
    std::string cle;
    while (in >> cle) {
        if (!cle.empty() && cle[0] == '#') {
            std::getline(in, cle);
            continue;
        }
        if (cle == "pas_m") {
            in >> g.pasX; /* jeu v1 : un seul pas, isotrope */
            g.pasZ = g.pasX;
        } else if (cle == "pas_x") {
            in >> g.pasX;
        } else if (cle == "pas_z") {
            in >> g.pasZ;
        } else if (cle == "colonnes") {
            in >> g.colonnes;
        } else if (cle == "rangees") {
            in >> g.rangees;
        } else {
            std::getline(in, cle);
        }
    }
    g.valide = g.pasX > 0.0f && g.pasZ > 0.0f && g.colonnes > 0 && g.rangees > 0;
    return g;
}

bool ecrireIndexRelief(const std::filesystem::path& sortie,
                       const std::string&           nomCarte,
                       const CalageCarte&           carte,
                       const GrilleRelief&          grille) {
    std::ofstream out(sortie / render::relief::NOM_INDEX, std::ios::trunc);
    if (!out) {
        return false;
    }
    out << "# Tuiles de relief Artouste - " << nomCarte << "\n";
    out << "# Grille ancrée sur le coin nord-ouest de la tuile (0, 0), en\n";
    out << "# coordonnées monde (X est, Z sud). Une tuile par fichier :\n";
    out << "# <rangée>/<colonne>.r16, 16 bits par point (voir fetch_relief.py).\n";
    out << "tuile_points " << RELIEF_TUILE_POINTS << "\n";
    /* Neuf chiffres significatifs : c'est ce qu'il faut pour relire EXACTEMENT le
       flottant écrit, et donc pour que l'index et l'en-tête des tuiles annoncent
       le même pas. */
    char pas[64];
    std::snprintf(pas, sizeof(pas), "pas_x %.9g\npas_z %.9g\n", static_cast<double>(grille.pasX),
                  static_cast<double>(grille.pasZ));
    out << pas;
    out << "colonnes " << grille.colonnes << "\n";
    out << "rangees " << grille.rangees << "\n";
    char coins[64];
    std::snprintf(coins, sizeof(coins), "coin_x %.2f\ncoin_z %.2f\n",
                  static_cast<double>(carte.originX - 0.5f * carte.largeurM),
                  static_cast<double>(carte.originZ - 0.5f * carte.hauteurM));
    out << coins;
    return out.good();
}

Estimation estimerRelief(const std::filesystem::path& dossierCarte) {
    Estimation         est;
    const GrilleRelief g = grilleRelief(dossierCarte);
    if (!g.valide) {
        est.detail = "Maillage de la carte illisible : le pas du relief ne peut pas être "
                     "calé dessus.";
        return est;
    }
    est.valide   = true;
    est.colonnes = g.colonnes;
    est.rangees  = g.rangees;
    est.blocs    = ((g.colonnes + RELIEF_TUILES_PAR_BLOC - 1) / RELIEF_TUILES_PAR_BLOC) *
                ((g.rangees + RELIEF_TUILES_PAR_BLOC - 1) / RELIEF_TUILES_PAR_BLOC);

    const double points = static_cast<double>(g.tuiles()) *
                          static_cast<double>(RELIEF_TUILE_POINTS) *
                          static_cast<double>(RELIEF_TUILE_POINTS);
    est.octetsDisque = static_cast<std::uintmax_t>(points * 2.0);
    /* Sur la ligne : quatre octets par pixel demandé, et RELIEF_SUR_ECH au carré
       pixels par point. C'est un plafond, les tuiles sans donnée LiDAR étant
       reçues puis jetées. */
    est.octetsReseau = static_cast<std::uintmax_t>(
        points * 4.0 * static_cast<double>(RELIEF_SUR_ECH * RELIEF_SUR_ECH));

    /* Durée : la ligne commande, il n'y a rien à compresser ici. Un mégaoctet
       par seconde, comme pour l'orthophoto ; la mesure prend le relais dès le
       premier bloc terminé. */
    const double minutes = std::round(
        std::max(static_cast<double>(est.octetsReseau) / (1e6 * 60.0), 1.0));
    char duree[64];
    if (minutes >= 60.0) {
        std::snprintf(duree, sizeof(duree), "environ %.0f h %02.0f", std::floor(minutes / 60.0),
                      minutes - 60.0 * std::floor(minutes / 60.0));
    } else {
        std::snprintf(duree, sizeof(duree), "environ %.0f minutes", minutes);
    }

    char phrase[448];
    std::snprintf(phrase, sizeof(phrase),
                  "%d x %d tuiles de %d points à %.2f x %.2f m : %s sur le disque, "
                  "environ %s à télécharger, en %d blocs. Compter %s.",
                  g.colonnes, g.rangees, RELIEF_TUILE_POINTS, static_cast<double>(g.pasX),
                  static_cast<double>(g.pasZ), formaterOctets(est.octetsDisque).c_str(),
                  formaterOctets(est.octetsReseau).c_str(), est.blocs, duree);
    est.detail = phrase;
    return est;
}

} /* namespace artouste::app::cartes */
