/*
 * EcranCartesBoutons.cpp
 * Réglages de la carte et actions sur le disque (voir EcranCartesInterne.hpp).
 *
 * Les boutons qui ne s'appliquent pas sont grisés plutôt que retirés : les
 * faire apparaître faisait bondir la largeur de la fenêtre d'une carte à
 * l'autre.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/cartes/EcranCartesInterne.hpp"

#include "app/cartes/FabriqueRelief.hpp"
#include "app/menu/cartes/EcranCartesRegles.hpp"

#include <imgui.h>

namespace artouste::app::ecran_cartes {

void dessinerReglages(Etat& etat) {
    cartes::EtatCarte& choisie = etat.courante();

    if (ImGui::Button(choisie.arbres ? "Arbres : oui" : "Arbres : non", bouton(150.0f))) {
        choisie.arbres       = !choisie.arbres;
        choisie.arbresDefini = true;
        ecrireOptions(choisie);
    }
    ImGui::SameLine();
    if (ImGui::Button(choisie.batiments ? "Bâtiments : oui" : "Bâtiments : non",
                      bouton(170.0f))) {
        choisie.batiments       = !choisie.batiments;
        choisie.batimentsDefini = true;
        ecrireOptions(choisie);
    }
    ImGui::SameLine();

    /* Sans tuiles sur le disque, le bouton ne dit pas "oui" : ce réglage
       n'allume rien. */
    ImGui::BeginDisabled(choisie.octetsTuiles == 0);
    const char* etiquetteTuiles = (choisie.octetsTuiles == 0) ? "Tuiles : aucune"
                                  : choisie.tuiles            ? "Tuiles : oui"
                                                              : "Tuiles : non";
    if (ImGui::Button(etiquetteTuiles, bouton(150.0f))) {
        choisie.tuiles        = !choisie.tuiles;
        choisie.tuilesDefinie = true;
        ecrireOptions(choisie);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!choisie.arbresDefini && !choisie.batimentsDefini &&
                         !choisie.tuilesDefinie);
    if (ImGui::Button("Réglages par défaut", bouton(200.0f))) {
        rendreAuDefaut(choisie, etat.arbresGeneral);
    }
    ImGui::EndDisabled();
}

void dessinerBoutonsDisque(Etat& etat) {
    cartes::EtatCarte& choisie = etat.courante();

    /* Un jeu entamé se reprend, même sur une carte qui n'aurait rien à gagner à
       en recevoir un neuf : le laisser à moitié écrit serait le pire des états. */
    ImGui::BeginDisabled(!cartes::reseauDisponible() ||
                         (!choisie.interet.vaut && !choisie.tuilesInachevees));
    if (ImGui::Button(choisie.tuilesInachevees ? "Reprendre la fabrication"
                                               : "Fabriquer les tuiles",
                      bouton(200.0f))) {
        etat.surRelief  = false;
        etat.estimation = cartes::estimer(choisie.dossier, finesseAFabriquer(choisie));
        etat.aFabriquer = static_cast<int>(etat.selection);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(choisie.octetsTuiles == 0);
    if (ImGui::Button("Supprimer les tuiles", bouton(200.0f))) {
        etat.surRelief  = false;
        etat.aSupprimer = static_cast<int>(etat.selection);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    if (ImGui::Button("Retour", bouton(150.0f))) {
        etat.fini = true;
    }

    /* Le relief sur sa propre ligne : toute carte peut en recevoir, il n'y a pas
       d'intérêt à mesurer comme pour l'image. */
    ImGui::BeginDisabled(!cartes::reseauDisponible());
    if (ImGui::Button(choisie.reliefInacheve ? "Reprendre relief 3D (L)"
                                             : "Fabriquer relief 3D (L)",
                      bouton(200.0f))) {
        etat.surRelief  = true;
        etat.estimation = cartes::estimerRelief(choisie.dossier);
        etat.aFabriquer = static_cast<int>(etat.selection);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(choisie.octetsRelief == 0);
    if (ImGui::Button("Supprimer relief 3D (Maj+Suppr)", bouton(260.0f))) {
        etat.surRelief  = true;
        etat.aSupprimer = static_cast<int>(etat.selection);
    }
    ImGui::EndDisabled();

    if (!cartes::reseauDisponible()) {
        ImGui::TextDisabled("Compilé sans libcurl : la fabrication est indisponible.");
    }
}

} /* namespace artouste::app::ecran_cartes */
