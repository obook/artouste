/*
 * EcranCartesInterne.hpp
 * Ce que les blocs d'affichage du gestionnaire de cartes se passent entre eux.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/menu/cartes/EcranCartes.hpp"
#include "ui/HudWidgets.hpp"

#include <imgui.h>

namespace artouste::app::ecran_cartes {

/* Largeur commune des boutons, à l'échelle du HUD. */
[[nodiscard]] inline ImVec2 bouton(float largeur) {
    return ImVec2(ui::hud_widgets::sc(largeur), 0.0f);
}

/* Annonce avant fabrication, image ou relief selon surRelief : place occupée,
   volume, durée probable. Défini dans EcranCartesAnnonce.cpp. */
void dessinerAnnonce(Etat& etat);

/* Réglages de la carte, puis les actions qui touchent au disque et la sortie.
   Définis dans EcranCartesBoutons.cpp. */
void dessinerReglages(Etat& etat);
void dessinerBoutonsDisque(Etat& etat);

} /* namespace artouste::app::ecran_cartes */
