/*
 * ConfigCles.cpp
 * Les trois listes de clés : connues, retirées, renommées.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Config.hpp"

#include "app/config/ConfigInterne.hpp"

#include <map>
#include <set>
#include <string>

namespace artouste::app {

const std::set<std::string>& clesConnues() {
    /* Ordre sans importance : c'est un ensemble. Toute clé ajoutée au chargeur
       ci-dessous doit figurer ici ET dans assets/config.default.txt, faute de quoi
       les tests de cohérence échouent (voir tests/config_tests.cpp). */
    static const std::set<std::string> cles = {"terrain",
                                               "turbine_demarree",
                                               "demo",
                                               "arbres",
                                               "relief_fenetre",
                                               "relief_debug",
                                               "zone_hv",
                                               "verifier_maj",
                                               "radio_url",
                                               "soleil_vitesse",
                                               "lune_vitesse",
                                               "brume_debut",
                                               "brume_fin",
                                               "arbres_max",
                                               "tuiles_fenetre_px",
                                               "tuiles_dossier",
                                               "relief_sommets_max",
                                               "msaa"};
    return cles;
}

/* Options qui ont existé puis ont été retirées du jeu. Elles ne sont plus lues,
   mais leur ligne survit dans le config.txt de qui les avait : on l'ignore en
   silence plutôt que de crier à la clé inconnue. Une entrée ajoutée ici ne s'en
   retire jamais, comme pour les renommages.
     souffle (août 2026) : le souffle rotor est désormais toujours actif, son coût
     ne justifiait pas un interrupteur (quelques centaines de billboards). */
const std::set<std::string>& clesRetirees() {
    static const std::set<std::string> retirees = {"souffle"};
    return retirees;
}

const std::map<std::string, std::string>& clesRenommees() {
    /* Ancien nom -> nom actuel. Vide tant qu'aucune option n'a été renommée ; une
       entrée ajoutée ici ne s'en retire jamais, car il existera toujours quelque
       part un config.txt d'une version d'avant. Le nom actuel doit figurer dans
       clesConnues(), l'ancien n'y figure plus (les tests le vérifient). */
    static const std::map<std::string, std::string> renommages = {
        /* Juillet 2026 : clés uniformisées en français. */
        {"tree_max", "arbres_max"},
        {"sun_time_scale", "soleil_vitesse"},
    };
    return renommages;
}

} /* namespace artouste::app */
