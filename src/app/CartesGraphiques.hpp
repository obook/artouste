/*
 * CartesGraphiques.hpp
 * Énumération des cartes graphiques présentes sur la machine, pour l'option
 * --gpu. Sert au diagnostic : sur un portable à deux cartes, le rendu part par
 * défaut sur la puce intégrée et le simulateur plafonne à la moitié de la
 * fréquence de l'écran ; cette liste montre à l'utilisateur ce dont il dispose.
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::app {

/*
Affiche les cartes graphiques détectées, puis la marche à suivre pour lancer le
simulateur sur la carte dédiée. Sous un système autre que Linux, signale que
l'énumération n'y est pas disponible.
*/
void afficherCartesGraphiques();

/*
Nom court du vendeur de la carte qui fait le rendu, tiré de la chaîne renvoyée
par glGetString(GL_RENDERER) : "NVIDIA", "INTEL", "AMD", "LOGICIEL" pour un
rendu purement processeur. Destiné à l'affichage compact du HUD.
*/
const char* nomCourtGpu(const char* renderer);

/*
Vrai si le rendu se fait sur la puce intégrée alors qu'une carte dédiée est
présente : le simulateur plafonne alors à la moitié de la fréquence de l'écran.
Ne distingue pas les deux cartes d'une machine tout-AMD, où l'intégrée et la
dédiée se nomment pareil.
*/
bool renduSurCarteIntegree(const char* renderer);

}  /* namespace artouste::app */
