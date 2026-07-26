/*
 * Bc7.hpp
 * Compression d'une image RGBA en BC7, avec sa chaîne de mipmaps, pour le
 * cache d'orthophotos (voir Dds.hpp).
 *
 * Pourquoi ne pas laisser le pilote graphique le faire : passer un format
 * interne compressé à glTexImage2D fonctionne, mais l'encodeur du pilote
 * travaille sur un seul fil et sans budget de temps. Mesuré sur la carte
 * capbreton, 94 mégapixels, cela portait le chargement de 3 à 38 secondes, à
 * refaire à chaque lancement. Ici on compresse une fois, en parallèle sur tous
 * les cœurs, et on range le résultat dans un cache.
 *
 * L'encodage est découpé en bandes de lignes de blocs pour deux raisons :
 * alimenter une barre de progression, et laisser l'appelant annuler proprement
 * si l'utilisateur ferme la fenêtre en cours de préparation.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include <functional>
#include <optional>

#include "render/Dds.hpp"

namespace artouste::render::bc7 {

/* Appelé entre deux bandes avec la fraction déjà compressée, de 0 à 1.
   Renvoyer faux annule l'encodage : compresser() rend alors un optionnel vide
   et rien n'est écrit. L'appel se fait toujours depuis le fil appelant, jamais
   depuis un fil de travail : le rappel peut donc dessiner une image OpenGL
   sans précaution particulière. */
using Progression = std::function<bool(float)>;

/* Compresse une image RGBA 8 bits (4 octets par pixel, R en premier) en BC7,
   en générant la chaîne complète de mipmaps jusqu'au niveau 1x1.

   Renvoie un optionnel vide si les dimensions sont invalides ou si le rappel a
   demandé l'annulation. */
[[nodiscard]] std::optional<dds::Image> compresser(const unsigned char* pixelsRgba,
                                                   int                  largeur,
                                                   int                  hauteur,
                                                   const Progression&   progression);

}  /* namespace artouste::render::bc7 */
