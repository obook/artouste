/*
 * Livery.hpp
 * Livrées disponibles pour l'Alouette II. Petit en-tête partagé par le modèle
 * (render::LoadedHelicopter) et l'application, pour typer l'état de livrée sans
 * inclure tout le modèle dans l'en-tête de l'application.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::render {

/* Les quatre livrées, dans l'ordre de défilement de la touche L / bouton A :
   blanche -> Gendarmerie (bleu) -> armée de terre (olive) ->
   Protection civile (rouge) -> blanche. */
enum class Livery { Blanche, Gendarmerie, ArmeeDeTerre, ProtectionCivile };

}  /* namespace artouste::render */
