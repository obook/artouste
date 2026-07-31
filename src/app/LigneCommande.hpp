/*
 * LigneCommande.hpp
 * Options de lancement lues sur la ligne de commande : carte, point d'apparition
 * et cap. Elles servent à reprendre un vol là où on en a besoin sans convoyer
 * l'appareil depuis le pad de départ, ce qui coûtait plusieurs kilomètres à
 * chaque vérification de monument.
 *
 * Les variables d'environnement ARTOUSTE_* restent lues et gardent leur sens ;
 * une option de ligne de commande l'emporte sur la configuration et sur le menu,
 * mais pas sur la variable d'environnement correspondante, qui reste le dernier
 * mot pour les scripts.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */
#pragma once

#include <string>

namespace artouste::app {

/* Options de lancement. Les champs "a..." disent si l'option a été donnée, ce
   qu'une valeur par défaut ne permettrait pas de distinguer : --alt 0 (posé au
   sol) et l'absence de --alt n'ont pas le même sens. */
struct OptionsLancement {
    std::string carte;    /* --carte : nom du dossier sous assets/terrain/ */
    std::string monument; /* --monument : nom cherché dans monuments.txt */
    std::string lieu;     /* --lieu : nom cherché dans landmarks.txt */

    float lon = 0.0f; /* --lon / --lat : position WGS84 explicite */
    float lat = 0.0f;
    bool  aLonLat = false;

    /* --alt : hauteur d'apparition en mètres AU-DESSUS DU SOL, pas altitude
       absolue. 0 pose l'appareil sur le relief. */
    float altitude  = 0.0f;
    bool  aAltitude = false;

    /* --cap : cap boussole en degrés (0 = nord, 90 = est). Sans lui, l'appareil
       garde le cap de départ de la carte (clé start_heading). */
    float cap  = 0.0f;
    bool  aCap = false;

    bool aide   = false; /* --aide / --help : afficher l'aide et sortir */
    bool erreur = false; /* option inconnue ou valeur manquante */

    /* Vrai si un point d'apparition a été demandé, sous quelque forme que ce
       soit. C'est ce qui décide de placer l'appareil ailleurs qu'au pad, de
       sauter le menu de démarrage et de lancer la turbine : apparaître en l'air
       turbine arrêtée, c'est tomber. */
    [[nodiscard]] bool aPointDapparition() const noexcept {
        return aLonLat || !monument.empty() || !lieu.empty();
    }

    /* Vrai si le menu de démarrage n'a plus lieu d'être : la carte ou le point
       d'apparition sont déjà choisis, le menu ne ferait que les redemander. */
    [[nodiscard]] bool sauteMenu() const noexcept {
        return !carte.empty() || aPointDapparition();
    }
};

/* Lit les options. N'écrit rien sur la sortie : c'est à l'appelant d'afficher
   l'aide si `aide` est vrai, ou de sortir en erreur si `erreur` l'est. */
[[nodiscard]] OptionsLancement lireLigneCommande(int argc, char** argv);

/* Normalise un nom pour la recherche : ne garde que lettres et chiffres, en
   minuscules, accents français retirés. "Sacré-Coeur" et "sacre coeur" donnent
   donc la même chaîne. Sans quoi il faudrait taper les accents exactement, au
   shell, pour trouver un monument. */
[[nodiscard]] std::string normaliserNom(const std::string& nom);

/* Affiche l'aide sur la sortie standard. `programme` est argv[0]. */
void afficherAide(const char* programme);

} /* namespace artouste::app */
