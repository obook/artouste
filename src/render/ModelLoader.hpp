// Chargement d'un fichier 3D (.ac FlightGear) en Model via Assimp.
// Parcourt la hiérarchie de nœuds en cumulant les transformations (les objets
// AC3D portent des décalages), résout les textures par rapport au dossier du
// fichier, et peut ignorer certains nœuds par sous-chaîne de nom (les plans
// semi-transparents blur/disc des rotors, les doublons HDR des vitrages).

#pragma once

#include "render/Model.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace artouste::render {

class ModelLoader {
public:
    // Renvoie un Model vide en cas d'échec (fichier absent, illisible).
    // transparentNameSubstrings : parties marquées translucides (verrière), à
    // dessiner en passe transparente.
    static Model load(const std::filesystem::path& path,
                      const std::vector<std::string>& skipNameSubstrings        = {},
                      const std::vector<std::string>& transparentNameSubstrings = {});
};

}  // namespace artouste::render
