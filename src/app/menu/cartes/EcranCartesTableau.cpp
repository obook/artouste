/*
 * EcranCartesTableau.cpp
 * En-tête et tableau du gestionnaire de cartes (voir EcranCartes.hpp).
 *
 * Règle de l'écran : annoncer avant d'agir. Chaque ligne porte sa taille,
 * l'espace libre est affiché en permanence.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/cartes/EcranCartes.hpp"

#include "app/menu/cartes/EcranCartesMesures.hpp"
#include "app/menu/cartes/EcranCartesRegles.hpp"

#include <imgui.h>

#include <system_error>

namespace artouste::app::ecran_cartes {

void dessinerEntete(const Etat& etat) {
    /* Les bâtiments sont compris dans le socle : les ajouter compterait deux fois. */
    std::uintmax_t total = 0;
    for (const cartes::EtatCarte& carte : etat.cartes) {
        total += carte.octetsSocle + carte.octetsTuiles + carte.octetsRelief;
    }
    ImGui::Text("Cartes installées : %s au total", formaterOctets(total).c_str());

    /* Chaque disque concerné : les tuiles vivent souvent ailleurs que le jeu
       (ARTOUSTE_TUILES), un seul chiffre tromperait. */
    std::error_code ec;
    const auto      place = std::filesystem::space(etat.assets, ec);
    if (!ec) {
        ImGui::SameLine();
        ImGui::TextDisabled("   jeu : %s libres", formaterOctets(place.available).c_str());
    }

    std::filesystem::path racineTuiles;
    for (const cartes::EtatCarte& carte : etat.cartes) {
        if (!carte.dossierTuiles.empty() && carte.dossierTuiles.parent_path() != carte.dossier) {
            racineTuiles = carte.dossierTuiles.parent_path();
            break;
        }
    }
    if (racineTuiles.empty()) {
        return;
    }
    std::error_code ecTuiles;
    const auto      placeTuiles = std::filesystem::space(racineTuiles, ecTuiles);
    if (!ecTuiles && placeTuiles.available != place.available) {
        ImGui::SameLine();
        ImGui::TextDisabled("   tuiles (%s) : %s libres",
                            racineTuiles.filename().string().c_str(),
                            formaterOctets(placeTuiles.available).c_str());
    }
}

void dessinerTableau(const Etat& etat) {
    if (!ImGui::BeginTable("cartes", 7,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        return;
    }
    ImGui::TableSetupColumn("Carte");
    ImGui::TableSetupColumn("Résolution");
    ImGui::TableSetupColumn("Socle");
    ImGui::TableSetupColumn("Tuiles");
    ImGui::TableSetupColumn("Relief");
    ImGui::TableSetupColumn("Bâtiments");
    ImGui::TableSetupColumn("Arbres");
    ImGui::TableHeadersRow();

    for (std::size_t i = 0; i < etat.cartes.size(); ++i) {
        const cartes::EtatCarte& c = etat.cartes[i];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        /* La ligne en cours est celle que pilotent les touches. */
        if (i == etat.selection) {
            ImGui::TextUnformatted(">");
            ImGui::SameLine();
        }
        ImGui::TextUnformatted(c.dir.c_str());

        ImGui::TableNextColumn();
        /* Le relief en suffixe, en trois états : sans jeu la carte reste plate,
           un jeu interrompu ne couvre qu'une part de l'emprise et le reste garde
           la maille d'ensemble. */
        const char* relief = (c.octetsRelief == 0) ? " 2D"
                             : c.reliefInacheve    ? " 3D (partiel)"
                             : c.reliefAutrePas    ? " 3D (à refaire)"
                                                   : " 3D";
        /* Des tuiles que le moteur écarte ne font pas une carte HR, si lourdes
           soient-elles : la ligne démentirait le vol. */
        if (!tuilesEfficaces(c)) {
            ImGui::TextDisabled("LR%s", relief);
        } else if (c.tuilesInachevees) {
            ImGui::TextDisabled("HR (partiel)%s", relief);
        } else if (c.tuiles) {
            ImGui::Text("HR%s", relief);
        } else {
            ImGui::TextDisabled("HR (éteintes)%s", relief);
        }

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(formaterOctets(c.octetsSocle).c_str());

        ImGui::TableNextColumn();
        /* Une croix, pas un tiret : cette carte n'aura jamais de tuiles, son
           orthophoto est déjà à la finesse de la source. */
        if (c.octetsTuiles == 0 && !c.interet.vaut) {
            ImGui::TextUnformatted("x");
        } else {
            ImGui::TextUnformatted(formaterOctets(c.octetsTuiles).c_str());
        }

        ImGui::TableNextColumn();
        /* Le relief fin est un poids comme les tuiles, et il n'y a pas de carte
           à qui il n'apporte rien : toutes ont un maillage à raffiner. */
        if (c.reliefInacheve) {
            ImGui::TextDisabled("%s (partiel)", formaterOctets(c.octetsRelief).c_str());
        } else if (c.reliefAutrePas) {
            ImGui::TextDisabled("%s (à refaire)", formaterOctets(c.octetsRelief).c_str());
        } else if (c.octetsRelief > 0) {
            ImGui::TextUnformatted(formaterOctets(c.octetsRelief).c_str());
        } else {
            ImGui::TextDisabled("-");
        }

        ImGui::TableNextColumn();
        /* Un état, pas un poids : les éteindre ne rend aucun octet. Leur taille
           est comptée dans le socle. */
        if (c.octetsBatiments == 0) {
            ImGui::TextDisabled("aucun");
        } else if (c.batiments) {
            ImGui::TextUnformatted("oui");
        } else {
            ImGui::TextDisabled("non");
        }

        ImGui::TableNextColumn();
        /* Les arbres coûtent des images par seconde, pas des mégaoctets. */
        if (c.arbres) {
            ImGui::TextUnformatted("oui");
        } else {
            ImGui::TextDisabled("non");
        }
    }
    ImGui::EndTable();
}

} /* namespace artouste::app::ecran_cartes */
