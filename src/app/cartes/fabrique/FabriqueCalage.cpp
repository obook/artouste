/*
 * FabriqueCalage.cpp
 * Calage d'une carte, grille de tuiles, intérêt et estimation d'une fabrication.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/cartes/fabrique/FabriqueInterne.hpp"

#include "app/cartes/FabriqueTuiles.hpp"

#include "render/tuiles/Pyramide.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace artouste::app::cartes {

[[nodiscard]] CalageCarte lireCalage(const std::filesystem::path& dossierCarte) {
    CalageCarte c;
    std::ifstream in(dossierCarte / "terrain.txt");
    if (!in) {
        return c;
    }
    bool aLargeur = false, aHauteur = false, aGeo0 = false, aGeo1 = false, aGeo2 = false,
         aGeo3 = false;
    std::string cle;
    while (in >> cle) {
        if (!cle.empty() && cle[0] == '#') {
            std::getline(in, cle);
            continue;
        }
        if (cle == "width_m") {
            in >> c.largeurM, aLargeur = true;
        } else if (cle == "height_m") {
            in >> c.hauteurM, aHauteur = true;
        } else if (cle == "origin_x") {
            in >> c.originX;
        } else if (cle == "origin_z") {
            in >> c.originZ;
        } else if (cle == "lon_min") {
            in >> c.lonMin, aGeo0 = true;
        } else if (cle == "lon_max") {
            in >> c.lonMax, aGeo1 = true;
        } else if (cle == "lat_min") {
            in >> c.latMin, aGeo2 = true;
        } else if (cle == "lat_max") {
            in >> c.latMax, aGeo3 = true;
        } else if (cle == "ortho_height") {
            in >> c.orthoHauteur;
        } else {
            std::getline(in, cle);
        }
    }
    c.valide = aLargeur && aHauteur && aGeo0 && aGeo1 && aGeo2 && aGeo3 && c.largeurM > 0.0f &&
               c.hauteurM > 0.0f;
    return c;
}

/* Grille de tuiles couvrant l'emprise. MÊME calcul que l'outil de découpage
   (src/tools/orthotuiles.cpp) et que le script Python : les trois doivent tomber
   d'accord, sans quoi les tuiles ne se rangeraient pas aux mêmes indices. */
[[nodiscard]] render::tuiles::Calage grille(const CalageCarte& carte, float mParPixel) {
    render::tuiles::Calage g;
    g.tuilePx   = TUILE_PX;
    g.mParPixel = mParPixel;
    const float tuileM = static_cast<float>(TUILE_PX) * mParPixel;
    g.colonnes  = static_cast<int>(std::ceil(carte.largeurM / tuileM));
    g.rangees   = static_cast<int>(std::ceil(carte.hauteurM / tuileM));
    g.coinX     = carte.originX - 0.5f * carte.largeurM;
    g.coinZ     = carte.originZ - 0.5f * carte.hauteurM;
    return g;
}

[[nodiscard]] std::string formaterOctets(std::uintmax_t octets) {
    char tampon[32];
    if (octets >= 1000ull * 1000ull * 1000ull) {
        std::snprintf(tampon, sizeof(tampon), "%.1f Go", static_cast<double>(octets) / 1e9);
    } else {
        std::snprintf(tampon, sizeof(tampon), "%.0f Mo", static_cast<double>(octets) / 1e6);
    }
    return tampon;
}

bool fabricationInachevee(const std::filesystem::path& dossierTuiles) {
    if (dossierTuiles.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(dossierTuiles / NOM_MARQUEUR_INACHEVE, ec);
}

Interet interet(const std::filesystem::path& dossierCarte) {
    Interet i;
    const CalageCarte carte = lireCalage(dossierCarte);
    if (!carte.valide || carte.orthoHauteur <= 0) {
        i.visee = FINESSE_LA_PLUS_GROSSIERE;
        i.vaut  = true;
        return i;
    }
    i.ortho = carte.hauteurM / static_cast<float>(carte.orthoHauteur);
    i.visee = std::clamp(i.ortho / GAIN_VISE, FINESSE_LA_PLUS_FINE, FINESSE_LA_PLUS_GROSSIERE);
    /* Une carte dont l'orthophoto est déjà à la finesse de la source ne peut rien
       gagner : c'est le cas des petites cartes découpées dans une image fine. */
    i.vaut = i.ortho >= i.visee * GAIN_MINIMUM;
    return i;
}

Estimation estimer(const std::filesystem::path& dossierCarte, float mParPixel) {
    Estimation est;
    const CalageCarte carte = lireCalage(dossierCarte);
    if (!carte.valide || mParPixel <= 0.0f) {
        est.detail = "Calage de carte illisible.";
        return est;
    }

    const render::tuiles::Calage g = grille(carte, mParPixel);
    est.valide   = g.valide();
    est.colonnes = g.colonnes;
    est.rangees  = g.rangees;
    const double pixels = static_cast<double>(g.colonnes) * static_cast<double>(g.rangees) *
                          static_cast<double>(TUILE_PX) * static_cast<double>(TUILE_PX);
    est.octetsDisque = static_cast<std::uintmax_t>(pixels * OCTETS_PAR_PIXEL);
    est.octetsReseau = static_cast<std::uintmax_t>(pixels * OCTETS_JPEG_PAR_PIXEL);
    est.blocs = ((g.colonnes + TUILES_PAR_BLOC - 1) / TUILES_PAR_BLOC) *
                ((g.rangees + TUILES_PAR_BLOC - 1) / TUILES_PAR_BLOC);

    /* Durée : deux plafonds, et c'est le plus haut qui commande. Celui de la
       ligne est une fourchette, on ne connaît pas son débit avant d'avoir reçu
       quelque chose. Celui de la MACHINE est plus régulier : découper un bloc et
       compresser ses tuiles en BC7 tient une cadence à peu près constante, et sur
       une carte fine il domine largement le téléchargement. L'annoncer évite de
       promettre dix minutes pour un travail de quatre heures. La mesure prend le
       relais dès le premier bloc terminé, elle englobe les deux. */
    const double minutesLigne = static_cast<double>(est.octetsReseau) / (1e6 * 60.0);
    const double minutesMachine =
        static_cast<double>(g.colonnes) * static_cast<double>(g.rangees) /
        (TUILES_PAR_SECONDE * 60.0);
    /* Arrondi AVANT de choisir l'unité : sans cela 59,7 minutes s'afficheraient
       "environ 60 minutes", une durée qui se dit en heures. */
    const double minutes =
        std::round(std::max(std::max(minutesLigne, minutesMachine), 1.0));

    char duree[64];
    if (minutes >= 60.0) {
        std::snprintf(duree, sizeof(duree), "environ %.0f h %02.0f", std::floor(minutes / 60.0),
                      minutes - 60.0 * std::floor(minutes / 60.0));
    } else {
        std::snprintf(duree, sizeof(duree), "environ %.0f minutes", minutes);
    }

    char phrase[448];
    std::snprintf(phrase, sizeof(phrase),
                  "%d x %d tuiles à %.2f m/px : %s sur le disque, environ %s à télécharger, "
                  "en %d blocs. Compter %s, la compression pesant autant que la ligne.",
                  g.colonnes, g.rangees, static_cast<double>(mParPixel),
                  formaterOctets(est.octetsDisque).c_str(),
                  formaterOctets(est.octetsReseau).c_str(), est.blocs, duree);
    est.detail = phrase;
    return est;
}

} /* namespace artouste::app::cartes */
