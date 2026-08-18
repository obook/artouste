/*
 * EcranCartesDetail.cpp
 * Détail de la carte choisie (voir EcranCartes.hpp).
 *
 * Ce qui décide de la netteté du sol est le RAPPORT entre la finesse des
 * tuiles et celle de l'orthophoto. Sans lui, on peut télécharger un gigaoctet
 * qui ne change rien à l'image.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/cartes/EcranCartes.hpp"

#include "app/menu/cartes/EcranCartesRegles.hpp"

#include <imgui.h>

#include <string>

namespace artouste::app::ecran_cartes {

namespace {

/* Réglages hérités de la configuration générale. Sans cette ligne, rien ne les
   distingue d'un choix pris ici. */
[[nodiscard]] std::string reglagesHerites(const cartes::EtatCarte& carte) {
    std::string herites;
    if (!carte.arbresDefini) {
        herites += "arbres";
    }
    if (!carte.batimentsDefini) {
        herites += herites.empty() ? "bâtiments" : ", bâtiments";
    }
    if (!carte.tuilesDefinie && carte.octetsTuiles > 0) {
        herites += herites.empty() ? "tuiles" : ", tuiles";
    }
    return herites;
}

/* Où sont les tuiles, ou pourquoi il n'y en a pas. */
void dessinerEmplacementTuiles(const cartes::EtatCarte& carte) {
    if (carte.octetsTuiles > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("Tuiles : %s", carte.dossierTuiles.string().c_str());
        ImGui::PopStyleColor();
    } else if (carte.interet.vaut) {
        ImGui::TextDisabled("Pas de tuiles de détail : le sol reste flou au ras du sol.");
    } else {
        ImGui::TextDisabled("Pas de tuiles de détail : l'orthophoto d'ensemble suffit ici.");
    }
}

/* Où est le relief fin, ou pourquoi il n'y en a pas. Sans lui la carte n'a que
   son maillage d'ensemble, plafonné par relief_sommets_max. */
void dessinerEmplacementRelief(const cartes::EtatCarte& carte) {
    if (carte.octetsRelief > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("Relief fin : %s", carte.dossierRelief.string().c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("Pas de relief fin : le sol garde la maille d'ensemble.");
    }
}

/* La comparaison qui dit si ces mégaoctets servent. */
void dessinerFinesses(const cartes::EtatCarte& carte) {
    if (carte.interet.ortho <= 0.0f || carte.finesseTuiles <= 0.0f) {
        return;
    }
    const float gain = carte.interet.ortho / carte.finesseTuiles;
    if (tuilesEfficaces(carte)) {
        ImGui::TextDisabled("Orthophoto %.2f m/px, tuiles %.2f m/px : %.1f fois plus "
                            "net au ras du sol.",
                            static_cast<double>(carte.interet.ortho),
                            static_cast<double>(carte.finesseTuiles),
                            static_cast<double>(gain));
        return;
    }
    /* En clair : cette ligne explique pourquoi le bouton reste éteint. */
    ImGui::TextWrapped("Orthophoto %.2f m/px, tuiles %.2f m/px : pas plus fines, "
                       "le moteur les ignore. À refaire ou à supprimer.",
                       static_cast<double>(carte.interet.ortho),
                       static_cast<double>(carte.finesseTuiles));
}

/* Ce qu'une fabrication apporterait, faute de tuiles. */
void dessinerProposition(const cartes::EtatCarte& carte) {
    if (carte.interet.ortho <= 0.0f || carte.finesseTuiles > 0.0f) {
        return;
    }
    if (!carte.interet.vaut) {
        ImGui::TextWrapped("Rien à fabriquer ici : l'orthophoto est déjà à %.2f m/px, la "
                           "finesse de la source. Des tuiles ne rendraient pas le sol "
                           "plus net.",
                           static_cast<double>(carte.interet.ortho));
        return;
    }
    ImGui::TextDisabled("Orthophoto %.2f m/px ; des tuiles à %.2f m/px la rendraient "
                        "%.1f fois plus nette.",
                        static_cast<double>(carte.interet.ortho),
                        static_cast<double>(carte.interet.visee),
                        static_cast<double>(carte.interet.ortho / carte.interet.visee));
}

} /* namespace */

void dessinerDetail(const Etat& etat, const cartes::Avancement& av) {
    const cartes::EtatCarte& carte = etat.courante();

    ImGui::TextWrapped("%s -- %s", carte.dir.c_str(), carte.titre.c_str());

    /* Pendant une fabrication l'inventaire date d'avant : mieux vaut se taire
       qu'annoncer "pas de tuiles" au-dessus d'un "terminé, 2371 tuiles". */
    if (etat.fabrique.enCours() || av.termine ||
        etat.aFabriquer == static_cast<int>(etat.selection)) {
        return;
    }

    const std::string herites = reglagesHerites(carte);
    if (!herites.empty()) {
        ImGui::TextDisabled("Suit la configuration générale : %s", herites.c_str());
    }
    dessinerEmplacementTuiles(carte);
    dessinerEmplacementRelief(carte);

    /* À dire avant toute comparaison de finesse : la carte n'est couverte qu'en
       partie. */
    if (carte.tuilesInachevees) {
        ImGui::TextWrapped("Fabrication interrompue : %d tuiles écrites sur %d. "
                           "Relancer reprendra où elle s'est arrêtée.",
                           carte.tuilesPresentes,
                           carte.tuilesAttendues);
    }
    if (carte.reliefAutrePas) {
        ImGui::TextWrapped("Relief à un autre pas (%.2f x %.2f m) : il ne s'emboîte pas dans "
                           "la maille de la carte, et la frontière de la fenêtre se voit en "
                           "vol. Le refabriquer efface celui-là et le refait au bon pas.",
                           static_cast<double>(carte.reliefPasX),
                           static_cast<double>(carte.reliefPasZ));
    }
    if (carte.reliefInacheve) {
        /* Le compte des tuiles écrites reste sous l'attendu même sur un jeu
           complet : celles qui sont hors couverture LiDAR ne sont pas écrites. */
        ImGui::TextWrapped("Relief interrompu : %d tuiles écrites sur %d attendues. "
                           "Relancer reprendra où il s'est arrêté.",
                           carte.reliefPresent,
                           carte.reliefAttendu);
    }
    dessinerFinesses(carte);

    if (tuilesDepassees(carte)) {
        /* En clair : c'est une proposition, pas un constat. */
        ImGui::TextWrapped("Ce jeu date d'avant la règle de finesse : %.2f m/px, alors "
                           "que cette carte vise %.2f. Le refaire rend le sol %.1f fois "
                           "plus net ; la relance efface d'abord l'ancien jeu, les deux "
                           "grilles ne pouvant cohabiter.",
                           static_cast<double>(carte.finesseTuiles),
                           static_cast<double>(carte.interet.visee),
                           static_cast<double>(carte.finesseTuiles / carte.interet.visee));
    }
    dessinerProposition(carte);
}

} /* namespace artouste::app::ecran_cartes */
