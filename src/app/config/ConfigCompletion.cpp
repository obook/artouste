/*
 * ConfigCompletion.cpp
 * Report des options nouvelles dans le config.txt de l'utilisateur.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/config/ConfigInterne.hpp"

#include "app/Config.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace artouste::app {

/* Ajoute à la fin de la configuration personnelle les options que le modèle
   connaît et qu'elle n'a pas. C'est le cas d'un fichier écrit par une version
   plus ancienne du simulateur : sans cela, une option nouvelle resterait à
   jamais invisible pour qui a déjà son config.txt, puisque le modèle n'est
   recopié qu'à la toute première exécution.

   Chaque option manquante est recopiée du modèle avec les commentaires qui la
   documentent, et avec la valeur qu'elle y porte -- celle que reçoit une
   installation neuve. Rien n'est réécrit : on n'ajoute qu'à la fin, les
   réglages existants ne sont jamais touchés.

   Le modèle est un simple fichier texte, posé à côté du jeu : l'utilisateur a
   pu l'effacer, l'écraser ou le remplir de n'importe quoi. On ne lui accorde donc
   aucune confiance : seules les options que le chargeur sait vraiment lire
   (clesConnues) sont recopiées, et tout le reste est signalé puis ignoré. Sans
   ce filtre, un modèle abîmé écrirait ses lignes parasites -- jusqu'à des octets
   binaires -- dans la configuration personnelle, qui n'a rien demandé.

   Le modèle du disque peut avoir été effacé. On ne complète alors rien et on le
   dit : les options nouvelles gardent la valeur par défaut du chargeur, le vol
   part normalement, et l'utilisateur sait quel fichier remettre en place.

   Si la configuration ne peut pas être écrite (jeu installé en lecture seule),
   on se contente de prévenir : ce n'est pas une erreur, la configuration lue
   reste valable et le vol part normalement. */
void completerDepuisModele(const std::filesystem::path& config,
                           const std::filesystem::path& modele,
                           const std::set<std::string>& clesPresentes) {
    std::string texte;
    {
        std::ifstream in(modele, std::ios::binary);
        texte.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        if (!in) {
            std::fprintf(stderr,
                         "[Config] %s illisible : les options nouvelles ne sont pas ajoutées "
                         "à la configuration, elles gardent leur valeur par défaut.\n",
                         modele.filename().string().c_str());
            return;
        }
    }

    std::vector<std::string> commentaires; /* lignes # qui précèdent la clé en cours */
    std::vector<std::string> ajoutees;     /* clés effectivement recopiées */
    std::vector<std::string> etrangeres;   /* clés du modèle que le jeu ne connaît pas */
    std::string blocs;                     /* texte à ajouter en fin de fichier */
    std::string ligne;
    std::istringstream flux(texte);
    while (std::getline(flux, ligne)) {
        if (!ligne.empty() && ligne.back() == '\r') {
            ligne.pop_back(); /* modèle enregistré en CRLF */
        }
        const std::string clean = trim(ligne);
        if (clean.empty()) {
            commentaires.clear(); /* une ligne vide ferme le bloc en cours */
            continue;
        }
        if (clean[0] == '#') {
            commentaires.push_back(ligne);
            continue;
        }
        const std::string cle = cleDeLigne(clean);
        /* Les commentaires ne valent que pour la clé qui suit : on les emporte ici
           et on remet le compteur à zéro pour le bloc suivant, quel que soit le
           sort de cette clé-ci. */
        const std::vector<std::string> documentation = commentaires;
        commentaires.clear();
        if (cle.empty()) {
            continue;
        }
        if (clesConnues().find(cle) == clesConnues().end()) {
            /* Le modèle décrit une option que ce jeu ne sait pas lire : modèle
               abîmé, ou venu d'une autre version. On la laisse où elle est. */
            etrangeres.push_back(cle);
            continue;
        }
        if (clesPresentes.find(cle) != clesPresentes.end()) {
            continue; /* l'utilisateur a déjà cette option, on n'y touche pas */
        }
        blocs += "\n";
        for (const std::string& commentaire : documentation) {
            blocs += commentaire + "\n";
        }
        blocs += ligne + "\n";
        ajoutees.push_back(cle);
    }
    if (!etrangeres.empty()) {
        std::string liste;
        for (const std::string& cle : etrangeres) {
            if (!liste.empty()) {
                liste += ", ";
            }
            /* Une clé venue d'un fichier abîmé peut contenir n'importe quoi : on
               n'en affiche qu'un extrait, et rien qui ne soit imprimable. */
            for (std::size_t i = 0; i < cle.size() && i < 24; ++i) {
                const unsigned char c = static_cast<unsigned char>(cle[i]);
                liste += (c >= 0x20 && c != 0x7F) ? cle[i] : '?';
            }
        }
        std::fprintf(stderr,
                     "[Config] %s : option(s) inconnue(s) de ce simulateur, non recopiée(s) "
                     "(modèle abîmé ou d'une autre version) : %s\n",
                     modele.filename().string().c_str(),
                     liste.c_str());
    }
    if (ajoutees.empty()) {
        return; /* configuration déjà complète : cas normal, on n'ouvre rien en écriture */
    }

    /* Fins de ligne du fichier de l'utilisateur : un fichier enregistré par le
       Bloc-notes de Windows est en CRLF, et y mêler des lignes en LF donnerait un
       fichier bâtard, illisible dans certains éditeurs. On lit donc l'existant tel
       quel (binaire) pour reprendre sa convention, et pour savoir s'il se termine
       bien par un saut de ligne. */
    std::string existant;
    {
        std::ifstream lecture(config, std::ios::binary);
        existant.assign(std::istreambuf_iterator<char>(lecture), std::istreambuf_iterator<char>());
    }
    const bool crlf = existant.find("\r\n") != std::string::npos;

    std::string ajout =
        "\n"
        "# ==========================================================================\n"
        "# Options apparues depuis que ce fichier a été créé, recopiées du modèle\n"
        "# config.default.txt telles qu'une installation neuve les reçoit. Modifiez-les,\n"
        "# déplacez-les ou commentez-les à votre guise ; une option effacée sera\n"
        "# simplement réajoutée ici au prochain lancement.\n"
        "# ==========================================================================\n" +
        blocs;
    if (crlf) {
        std::string converti;
        converti.reserve(ajout.size() + ajout.size() / 8);
        for (const char c : ajout) {
            if (c == '\n') {
                converti += '\r';
            }
            converti += c;
        }
        ajout.swap(converti);
    }
    if (!existant.empty() && existant.back() != '\n') {
        ajout.insert(0, crlf ? "\r\n" : "\n"); /* le fichier ne finissait pas par un saut */
    }

    std::ofstream out(config, std::ios::binary | std::ios::app);
    if (!out) {
        std::fprintf(stderr,
                     "[Config] %s non modifiable : les nouvelles options n'y ont pas été "
                     "ajoutées (elles gardent leur valeur par défaut).\n",
                     config.filename().string().c_str());
        return;
    }
    out << ajout;

    std::string liste;
    for (const std::string& cle : ajoutees) {
        if (!liste.empty()) {
            liste += ", ";
        }
        liste += cle;
    }
    std::printf("[Config] nouvelle(s) option(s) ajoutée(s) à %s : %s\n",
                config.filename().string().c_str(),
                liste.c_str());
}

} /* namespace artouste::app */
