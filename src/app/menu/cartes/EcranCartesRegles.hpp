/*
 * EcranCartesRegles.hpp
 * Règles du gestionnaire de cartes : ce que vaut un jeu de tuiles, où il
 * atterrit, comment les réglages d'une carte sont écrits.
 *
 * Le clavier et les boutons passent tous par ces fonctions : c'est ce qui
 * garde les deux chemins d'accord.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/cartes/EtatCarte.hpp"

#include <filesystem>

namespace artouste::app::ecran_cartes {

/* Tuiles plus fines que l'orthophoto d'ensemble, donc chargées par le moteur.
   Même question que render::tuiles::niveauUtile, et non celle de l'intérêt
   d'une fabrication, plus exigeante. */
[[nodiscard]] bool tuilesEfficaces(const cartes::EtatCarte& carte);

/* Jeu plus grossier que ce que la carte vise. Les premiers jeux datent d'avant
   la règle de finesse ; ils servent encore, mais refaits seraient plus nets.
   La marge de 20 % évite de signaler un écart d'arrondi. */
[[nodiscard]] bool tuilesDepassees(const cartes::EtatCarte& carte);

/* Finesse à demander : celle du jeu entamé s'il y en a un, sinon celle visée.
   Reprendre à une autre finesse laisserait deux grilles incompatibles. */
[[nodiscard]] float finesseAFabriquer(const cartes::EtatCarte& carte);

/* Dossier que le lancement va remplir : celui qui existe, sinon celui où le
   moteur ira chercher les tuiles. */
[[nodiscard]] std::filesystem::path destinationTuiles(const cartes::EtatCarte&     carte,
                                                      const std::filesystem::path& racineTuiles);

/* N'écrit que ce qui a été explicitement réglé pour cette carte : le reste
   continue de suivre la configuration générale. */
void ecrireOptions(const cartes::EtatCarte& carte);

/* Efface le options.txt de la carte : elle suivra de nouveau la configuration
   générale. Sans cela, un réglage pris une fois l'était pour toujours. */
void rendreAuDefaut(cartes::EtatCarte& carte, bool arbresGeneral);

} /* namespace artouste::app::ecran_cartes */
