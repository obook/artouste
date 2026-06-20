/*
 * ModelLoader.hpp
 * Charge un fichier 3D (format .ac de FlightGear) et le transforme en Model,
 * à l'aide de la bibliothèque Assimp. Le chargeur parcourt l'arbre des nœuds,
 * place chaque morceau au bon endroit, retrouve les textures dans le dossier du
 * fichier, et sait écarter certains nœuds repérés par leur nom.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Model.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace artouste::render {

class ModelLoader {
public:
    /* Charge le fichier et renvoie le modèle ; en cas d'échec (fichier absent
       ou illisible), renvoie un Model vide.
       - skipNameSubstrings : noms de nœuds à ignorer complètement (par exemple
         les plans flous du rotor en mouvement).
       - transparentNameSubstrings : noms de nœuds à traiter comme translucides
         (les vitrages), pour les dessiner dans la passe transparente. */
    static Model load(const std::filesystem::path& path,
                      const std::vector<std::string>& skipNameSubstrings        = {},
                      const std::vector<std::string>& transparentNameSubstrings = {});
};

}  /* namespace artouste::render */
