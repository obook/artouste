/*
 * EcranCartesAnnonce.cpp
 * Annonce avant une fabrication (voir EcranCartesInterne.hpp).
 *
 * Personne ne doit découvrir après coup qu'il vient de lancer deux gigaoctets :
 * l'écran dit ce que cela coûte, où cela atterrit, et ce qu'il restera sur ce
 * disque-là.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/menu/cartes/EcranCartesInterne.hpp"

#include "app/menu/cartes/EcranCartesMesures.hpp"
#include "app/menu/cartes/EcranCartesRegles.hpp"

#include <imgui.h>

#include <cmath>
#include <system_error>

namespace artouste::app::ecran_cartes {

namespace {

/* Ce qui est déjà sur le disque et ce qu'il en adviendra. */
void dessinerCeQuiEstLa(const Etat& etat, const cartes::EtatCarte& carte) {
    if (etat.surRelief) {
        if (carte.reliefInacheve) {
            ImGui::TextWrapped("Reprise : %d tuiles de relief déjà écrites (%s), les blocs "
                               "déjà traités sont sautés.",
                               carte.reliefPresent, formaterOctets(carte.octetsRelief).c_str());
        } else if (carte.reliefAutrePas) {
            ImGui::TextWrapped("Le relief en place (%.2f x %.2f m, %s) sera effacé : deux pas "
                               "ne se mélangent pas.",
                               static_cast<double>(carte.reliefPasX),
                               static_cast<double>(carte.reliefPasZ),
                               formaterOctets(carte.octetsRelief).c_str());
        } else if (carte.octetsRelief > 0) {
            ImGui::TextWrapped("Le relief en place (%s) est au bon pas : les blocs déjà "
                               "traités seront sautés.",
                               formaterOctets(carte.octetsRelief).c_str());
        }
        return;
    }
    if (carte.tuilesInachevees) {
        /* Ces chiffres décrivent le jeu ENTIER : dire ce qui est déjà là évite
           de croire qu'on va tout retélécharger. */
        ImGui::TextWrapped("Reprise : %d tuiles déjà écrites (%s), seules les manquantes "
                           "seront téléchargées.",
                           carte.tuilesPresentes, formaterOctets(carte.octetsTuiles).c_str());
    } else if (carte.finesseTuiles > 0.0f &&
               std::fabs(carte.finesseTuiles - finesseAFabriquer(carte)) > 1e-4f) {
        ImGui::TextWrapped("Les tuiles en place (%.2f m/px, %s) seront effacées : deux "
                           "finesses ne se mélangent pas.",
                           static_cast<double>(carte.finesseTuiles),
                           formaterOctets(carte.octetsTuiles).c_str());
    }
}

} /* namespace */

void dessinerAnnonce(Etat& etat) {
    const cartes::EtatCarte& carte = etat.courante();

    ImGui::TextWrapped("%s", etat.estimation.detail.c_str());
    dessinerCeQuiEstLa(etat, carte);

    /* Où cela atterrit et ce qu'il restera sur CE disque : sans ces lignes, on
       peut remplir son disque système en croyant écrire ailleurs. */
    const std::filesystem::path cible = etat.surRelief
                                            ? destinationRelief(carte, etat.racineTuiles)
                                            : destinationTuiles(carte, etat.racineTuiles);
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
                    formaterOctets(placeCible.available).c_str(), formaterOctets(apres).c_str());
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

} /* namespace artouste::app::ecran_cartes */
