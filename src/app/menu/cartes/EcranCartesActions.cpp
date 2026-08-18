/*
 * EcranCartesActions.cpp
 * Suivi de fabrication et confirmations (voir EcranCartes.hpp).
 *
 * Rien de destructeur ne part sans confirmation. L'annonce et les boutons, plus
 * longs, sont dans EcranCartesAnnonce.cpp et EcranCartesBoutons.cpp.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/cartes/EcranCartes.hpp"

#include "app/menu/cartes/EcranCartesInterne.hpp"
#include "app/menu/cartes/EcranCartesMesures.hpp"
#include "app/menu/cartes/EcranCartesRegles.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <cstdio>

namespace artouste::app::ecran_cartes {

namespace {

/* Fabrication en cours ou compte rendu affiché : rien d'autre ne se lance. */
void dessinerFabrication(Etat& etat, const cartes::Avancement& av) {
    ImGui::Text("%s", av.message.c_str());
    if (av.blocsTotal > 0) {
        const float part =
            static_cast<float>(av.blocsFaits) / static_cast<float>(av.blocsTotal);
        char etiquette[128];
        std::snprintf(etiquette, sizeof(etiquette), "%d / %d blocs, %d tuiles, %s",
                      av.blocsFaits, av.blocsTotal, av.tuilesEcrites,
                      formaterOctets(av.octetsEcrits).c_str());
        /* Le jaune des histogrammes rend le texte blanc d'ImGui illisible : on
           reprend le bleu des boutons. */
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.26f, 0.45f, 0.68f, 1.0f));
        ImGui::ProgressBar(part, bouton(420.0f), etiquette);
        ImGui::PopStyleColor();
    }

    /* Débit et durée seulement une fois mesurés : tant qu'aucun bloc n'est
       revenu, on ne sait rien. Le tourniquet, lui, tourne d'un bout à l'autre :
       c'est le seul élément qui bouge entre deux blocs. */
    if (etat.fabrique.enCours()) {
        const char rouet = caractereTournant(glfwGetTime());
        if (av.octetsParSeconde > 0.0) {
            ImGui::TextDisabled("%c Débit IGN : %s, encore %s", rouet,
                                formaterDebit(av.octetsParSeconde).c_str(),
                                formaterDuree(av.secondesRestantes).c_str());
        } else {
            ImGui::TextDisabled("%c Débit IGN : pas encore mesuré, premier bloc en cours.",
                                rouet);
        }
        if (ImGui::Button("Arrêter (Échap)", bouton(160.0f))) {
            etat.fabrique.annuler();
        }
        return;
    }
    if (ImGui::Button("Fermer (Entrée)", bouton(160.0f))) {
        etat.fabrique.oublier();
        etat.refaireInventaire    = true;
        etat.inventaireCarteSeule = true;
    }
}

void dessinerConfirmationSuppression(Etat& etat) {
    ImGui::TextUnformatted(etat.surRelief ? "Supprimer le relief 3D de cette carte ?"
                                          : "Supprimer les tuiles de cette carte ?");
    ImGui::SameLine();
    if (ImGui::Button("Confirmer (Entrée)", bouton(160.0f))) {
        supprimerTuiles(etat);
        etat.aSupprimer = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Annuler", bouton(120.0f))) {
        etat.aSupprimer = -1;
    }
}

} /* namespace */

void dessinerActions(Etat& etat, const cartes::Avancement& av) {
    if (etat.fabrique.enCours() || av.termine) {
        dessinerFabrication(etat, av);
    } else if (etat.aFabriquer == static_cast<int>(etat.selection)) {
        dessinerAnnonce(etat);
    } else if (etat.aSupprimer == static_cast<int>(etat.selection)) {
        dessinerConfirmationSuppression(etat);
    } else {
        dessinerReglages(etat);
        dessinerBoutonsDisque(etat);
    }
}

} /* namespace artouste::app::ecran_cartes */
