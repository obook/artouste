/*
 * EcranCartesActions.cpp
 * Suivi de fabrication, confirmations et boutons (voir EcranCartes.hpp).
 *
 * Rien de destructeur ne part sans confirmation, et l'annonce dit toujours ce
 * que l'action va coûter en place disque.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/cartes/EcranCartes.hpp"

#include "app/menu/cartes/EcranCartesMesures.hpp"
#include "app/menu/cartes/EcranCartesRegles.hpp"
#include "ui/HudWidgets.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <cmath>
#include <cstdio>
#include <system_error>

namespace artouste::app::ecran_cartes {

namespace {

/* Largeur commune des boutons, à l'échelle du HUD. */
[[nodiscard]] ImVec2 bouton(float largeur) {
    return ImVec2(ui::hud_widgets::sc(largeur), 0.0f);
}

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
        etat.refaireInventaire = true;
    }
}

/* Annonce avant fabrication : place occupée, volume, durée probable. Personne
   ne doit découvrir après coup qu'il vient de lancer deux gigaoctets. */
void dessinerAnnonce(Etat& etat) {
    const cartes::EtatCarte& carte = etat.courante();

    ImGui::TextWrapped("%s", etat.estimation.detail.c_str());
    if (carte.tuilesInachevees) {
        /* Ces chiffres décrivent le jeu ENTIER : dire ce qui est déjà là évite
           de croire qu'on va tout retélécharger. */
        ImGui::TextWrapped("Reprise : %d tuiles déjà écrites (%s), seules les manquantes "
                           "seront téléchargées.",
                           carte.tuilesPresentes,
                           formaterOctets(carte.octetsTuiles).c_str());
    } else if (carte.finesseTuiles > 0.0f &&
               std::fabs(carte.finesseTuiles - finesseAFabriquer(carte)) > 1e-4f) {
        ImGui::TextWrapped("Les tuiles en place (%.2f m/px, %s) seront effacées : deux "
                           "finesses ne se mélangent pas.",
                           static_cast<double>(carte.finesseTuiles),
                           formaterOctets(carte.octetsTuiles).c_str());
    }

    /* Où cela atterrit et ce qu'il restera sur CE disque : sans ces lignes, on
       peut remplir son disque système en croyant écrire ailleurs. */
    const std::filesystem::path cible = destinationTuiles(carte, etat.racineTuiles);
    ImGui::TextWrapped("Destination : %s", cible.string().c_str());

    /* Le dossier n'existe pas encore : on interroge son parent le plus proche. */
    std::filesystem::path sonde = cible;
    while (!sonde.empty() && !std::filesystem::exists(sonde)) {
        sonde = sonde.parent_path();
    }
    std::error_code ecPlace;
    const auto      placeCible = std::filesystem::space(sonde, ecPlace);
    if (!ecPlace) {
        const std::uintmax_t apres = (placeCible.available > etat.estimation.octetsDisque)
                                         ? placeCible.available - etat.estimation.octetsDisque
                                         : 0;
        ImGui::Text("Ce disque : %s libres, %s après",
                    formaterOctets(placeCible.available).c_str(),
                    formaterOctets(apres).c_str());
    }

    const bool tientSurLeDisque =
        ecPlace ||
        placeCible.available > etat.estimation.octetsDisque + 500ull * 1000ull * 1000ull;
    if (!tientSurLeDisque) {
        ImGui::TextUnformatted("Place insuffisante sur le disque de destination.");
    }
    if (tientSurLeDisque && etat.estimation.valide &&
        ImGui::Button("Lancer (Entrée)", bouton(160.0f))) {
        lancerFabrication(etat);
        etat.aFabriquer = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Renoncer", bouton(120.0f))) {
        etat.aFabriquer = -1;
    }
}

void dessinerConfirmationSuppression(Etat& etat) {
    ImGui::TextUnformatted("Supprimer les tuiles de cette carte ?");
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

/* Réglages de la carte. Les boutons qui ne s'appliquent pas sont grisés plutôt
   que retirés : les faire apparaître faisait bondir la largeur de la fenêtre
   d'une carte à l'autre. */
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

/* Ce qui touche au disque, et la sortie. */
void dessinerBoutonsDisque(Etat& etat) {
    cartes::EtatCarte& choisie = etat.courante();

    /* Un jeu entamé se reprend, même sur une carte qui n'aurait rien à gagner à
       en recevoir un neuf : le laisser à moitié écrit serait le pire des états. */
    ImGui::BeginDisabled(!cartes::reseauDisponible() ||
                         (!choisie.interet.vaut && !choisie.tuilesInachevees));
    if (ImGui::Button(choisie.tuilesInachevees ? "Reprendre la fabrication"
                                               : "Fabriquer les tuiles",
                      bouton(200.0f))) {
        etat.estimation = cartes::estimer(choisie.dossier, finesseAFabriquer(choisie));
        etat.aFabriquer = static_cast<int>(etat.selection);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(choisie.octetsTuiles == 0);
    if (ImGui::Button("Supprimer les tuiles", bouton(200.0f))) {
        etat.aSupprimer = static_cast<int>(etat.selection);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    if (ImGui::Button("Retour", bouton(150.0f))) {
        etat.fini = true;
    }
    if (!cartes::reseauDisponible()) {
        ImGui::TextDisabled("Compilé sans libcurl : la fabrication est indisponible.");
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
