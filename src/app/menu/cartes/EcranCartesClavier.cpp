/*
 * EcranCartesClavier.cpp
 * Touches du gestionnaire de cartes (voir EcranCartes.hpp).
 *
 * Le curseur est masqué en plein écran : tout doit être atteignable au
 * clavier. Entrée lance puis confirme, Échap annule puis ferme. Pas de F pour
 * "fabriquer" : elle bascule le plein écran partout dans le jeu.
 *
 * Le relief double les deux actions d'image : L pour le fabriquer, Maj+Suppr
 * pour le supprimer.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/cartes/EcranCartes.hpp"

#include "app/cartes/FabriqueRelief.hpp"
#include "app/menu/cartes/EcranCartesRegles.hpp"
#include "input/Keyboard.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>

namespace artouste::app::ecran_cartes {

namespace {

/* Vrai à l'image où la touche vient d'être enfoncée, et à celle-là seulement. */
[[nodiscard]] bool front(bool actuel, bool& precedent) {
    const bool debut = actuel && !precedent;
    precedent        = actuel;
    return debut;
}

} /* namespace */

void traiterClavier(GLFWwindow* fenetre, Etat& etat) {
    const bool haut    = glfwGetKey(fenetre, GLFW_KEY_UP) == GLFW_PRESS;
    const bool bas     = glfwGetKey(fenetre, GLFW_KEY_DOWN) == GLFW_PRESS;
    const bool retour  = glfwGetKey(fenetre, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    const bool valider = glfwGetKey(fenetre, GLFW_KEY_ENTER) == GLFW_PRESS ||
                         glfwGetKey(fenetre, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    const bool supprimer = glfwGetKey(fenetre, GLFW_KEY_DELETE) == GLFW_PRESS;
    /* Maj enfoncée : la suppression porte sur le relief, pas sur l'image. */
    const bool maj = glfwGetKey(fenetre, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                     glfwGetKey(fenetre, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    /* GLFW_KEY_A désigne une POSITION, celle qui écrit "q" en AZERTY. */
    const bool bascArbres    = glfwGetKey(fenetre, input::toucheImprimant('a')) == GLFW_PRESS;
    const bool bascBatiments = glfwGetKey(fenetre, input::toucheImprimant('b')) == GLFW_PRESS;
    const bool bascTuiles    = glfwGetKey(fenetre, input::toucheImprimant('t')) == GLFW_PRESS;
    const bool rendre        = glfwGetKey(fenetre, input::toucheImprimant('r')) == GLFW_PRESS;
    const bool bascRelief    = glfwGetKey(fenetre, input::toucheImprimant('l')) == GLFW_PRESS;

    const bool frontRetour    = front(retour, etat.fronts.retour);
    const bool frontValider   = front(valider, etat.fronts.valider);
    const bool frontSupprimer = front(supprimer, etat.fronts.supprimer);
    const bool frontArbres    = front(bascArbres, etat.fronts.arbres);
    const bool frontBatiments = front(bascBatiments, etat.fronts.batiments);
    const bool frontTuiles    = front(bascTuiles, etat.fronts.tuiles);
    const bool frontRendre    = front(rendre, etat.fronts.rendre);
    const bool frontRelief    = front(bascRelief, etat.fronts.relief);
    const bool frontHaut      = front(haut, etat.fronts.haut);
    const bool frontBas       = front(bas, etat.fronts.bas);

    cartes::EtatCarte& courante = etat.courante();

    if (etat.fabrique.enCours()) {
        /* Pendant la fabrication, la seule décision possible est de l'arrêter. */
        if (frontRetour) {
            etat.fabrique.annuler();
        }
        return;
    }

    if (etat.fabrique.avancement().termine) {
        /* Les deux touches ferment le compte rendu ; le disque a changé. */
        if (frontRetour || frontValider) {
            etat.fabrique.oublier();
            etat.refaireInventaire    = true;
            etat.inventaireCarteSeule = true;
        }
        return;
    }

    if (etat.aFabriquer >= 0 || etat.aSupprimer >= 0) {
        /* Une confirmation attend : Entrée confirme, Échap renonce. */
        if (frontValider && etat.aFabriquer >= 0) {
            /* Rien à lancer sur une estimation invalide : le bouton de l'écran
               reste caché dans ce cas, la touche doit faire de même. */
            if (etat.estimation.valide) {
                lancerFabrication(etat);
            }
            etat.aFabriquer = -1;
        } else if (frontValider && etat.aSupprimer >= 0) {
            supprimerTuiles(etat);
            etat.aSupprimer = -1;
        } else if (frontRetour) {
            etat.aFabriquer = -1;
            etat.aSupprimer = -1;
        }
        return;
    }

    if (frontHaut && etat.selection > 0) {
        --etat.selection;
    }
    if (frontBas && etat.selection + 1 < etat.cartes.size()) {
        ++etat.selection;
    }
    if (frontRetour) {
        etat.fini = true;
    }
    /* Entrée ouvre l'annonce, jamais le téléchargement. Une carte qui n'a rien
       à y gagner ne s'ouvre pas. */
    if (frontValider && cartes::reseauDisponible() &&
        (courante.interet.vaut || courante.tuilesInachevees)) {
        etat.surRelief  = false;
        etat.estimation = cartes::estimer(courante.dossier, finesseAFabriquer(courante));
        etat.aFabriquer = static_cast<int>(etat.selection);
    }
    /* L comme relief : toute carte peut en recevoir, il n'y a pas d'intérêt à
       mesurer comme pour l'image. */
    if (frontRelief && cartes::reseauDisponible()) {
        etat.surRelief  = true;
        etat.estimation = cartes::estimerRelief(courante.dossier);
        etat.aFabriquer = static_cast<int>(etat.selection);
    }
    if (frontSupprimer && (maj ? courante.octetsRelief > 0 : courante.octetsTuiles > 0)) {
        etat.surRelief  = maj;
        etat.aSupprimer = static_cast<int>(etat.selection);
    }
    if (frontArbres) {
        courante.arbres       = !courante.arbres;
        courante.arbresDefini = true;
        ecrireOptions(courante);
    }
    if (frontBatiments) {
        courante.batiments       = !courante.batiments;
        courante.batimentsDefini = true;
        ecrireOptions(courante);
    }
    if (frontRendre) {
        rendreAuDefaut(courante, etat.arbresGeneral);
    }
    /* Sans tuiles, la touche écrirait une option sans effet. */
    if (frontTuiles && courante.octetsTuiles > 0) {
        courante.tuiles        = !courante.tuiles;
        courante.tuilesDefinie = true;
        ecrireOptions(courante);
    }
}

} /* namespace artouste::app::ecran_cartes */
