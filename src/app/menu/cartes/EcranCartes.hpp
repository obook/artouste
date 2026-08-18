/*
 * EcranCartes.hpp
 * État du gestionnaire de cartes et découpage de son affichage.
 *
 * Les fonctions de dessin ne connaissent que la structure Etat, jamais
 * Application. Ce qui demande d'en sortir (refaire l'inventaire, prévenir le
 * reste du programme) passe par un drapeau que la boucle relève.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

#include "app/cartes/EtatCarte.hpp"
#include "app/cartes/FabriqueTuiles.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

struct GLFWwindow;

namespace artouste::app::ecran_cartes {

/* Touches maintenues à l'image précédente : une touche tenue ne doit agir
   qu'une fois. */
struct Fronts {
    bool haut      = false;
    bool bas       = false;
    bool retour    = false;
    bool valider   = false;
    bool supprimer = false;
    bool arbres    = false;
    bool batiments = false;
    bool tuiles    = false;
    bool rendre    = false;
    bool relief    = false;
};

/* Tout ce que l'écran garde d'une image à l'autre. */
struct Etat {
    std::vector<cartes::EtatCarte> cartes;
    std::size_t                    selection = 0;

    int  aSupprimer = -1; /* carte dont la suppression de tuiles est à confirmer */
    int  aFabriquer = -1; /* carte dont la fabrication est à confirmer */
    /* L'action en attente, en cours ou rendue porte-t-elle sur le RELIEF plutôt
       que sur l'image ? Les deux jeux passent par la même annonce, la même
       confirmation et la même fabrique : ce drapeau est ce qui les distingue. */
    bool surRelief  = false;
    bool fini       = false;

    /* Demandes à la boucle : elle seule sait refaire l'inventaire (il passe par
       l'écran d'attente) et prévenir le reste du programme. */
    bool refaireInventaire = false;
    /* Cet inventaire ne porte que sur la carte choisie : une fabrication ou une
       suppression n'a remanié qu'elle, et tout remesurer coûte une minute et
       demie quand les tuiles vivent sur un disque externe. */
    bool inventaireCarteSeule = false;
    bool disqueRemanie        = false;

    /* Tourne dans son propre fil, l'écran ne fait que la suivre. */
    cartes::Fabrique   fabrique;
    cartes::Estimation estimation;

    std::filesystem::path assets;
    std::filesystem::path racineTuiles;

    /* Ce que vaut un réglage qu'aucune carte n'a pris pour elle. */
    bool arbresGeneral = true;

    Fronts fronts;

    /* Carte sur laquelle portent les touches et les boutons. */
    [[nodiscard]] cartes::EtatCarte&       courante() { return cartes[selection]; }
    [[nodiscard]] const cartes::EtatCarte& courante() const { return cartes[selection]; }
};

/* Les deux actions qui touchent au disque, sur l'image ou sur le relief selon
   surRelief. Elles lèvent disqueRemanie dès le DÉPART : une fabrication arrêtée
   en route a posé ses premières tuiles. Définies dans EcranCartesRegles.cpp. */
void lancerFabrication(Etat& etat);
void supprimerTuiles(Etat& etat);

/* Touches de l'écran : déplacement dans la liste, réglages de la carte,
   confirmations, sortie. Défini dans EcranCartesClavier.cpp. */
void traiterClavier(GLFWwindow* fenetre, Etat& etat);

/* Total installé et espace libre. Défini dans EcranCartesTableau.cpp. */
void dessinerEntete(const Etat& etat);

/* Tableau des cartes, une ligne par carte. Défini dans EcranCartesTableau.cpp. */
void dessinerTableau(const Etat& etat);

/* Détail de la carte choisie. Défini dans EcranCartesDetail.cpp. */
void dessinerDetail(const Etat& etat, const cartes::Avancement& av);

/* Suivi de fabrication, confirmations et boutons d'action. Défini dans
   EcranCartesActions.cpp.

   L'avancement est passé en argument, et non relu ici : le détail et les
   actions doivent parler du même instant. */
void dessinerActions(Etat& etat, const cartes::Avancement& av);

} /* namespace artouste::app::ecran_cartes */
