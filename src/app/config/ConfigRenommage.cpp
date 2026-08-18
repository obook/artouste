/*
 * ConfigRenommage.cpp
 * Rend leur nom actuel aux options renommées, avant toute lecture.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/Config.hpp"

#include "app/config/ConfigInterne.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace artouste::app {

std::size_t renommerAnciennesCles(const std::filesystem::path& config,
                                  const std::map<std::string, std::string>& renommages) {
    if (renommages.empty()) {
        return 0; /* cas courant : aucune option n'a jamais été renommée */
    }
    std::string contenu;
    {
        std::ifstream in(config, std::ios::binary);
        if (!in) {
            return 0;
        }
        contenu.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    /* Découpage en lignes, fins de ligne comprises : tout ce qui n'est pas une clé
       à renommer doit ressortir octet pour octet, commentaires et présentation de
       l'utilisateur compris. */
    std::vector<std::string> lignes;
    for (std::size_t debut = 0; debut < contenu.size();) {
        const std::size_t saut = contenu.find('\n', debut);
        const std::size_t fin = (saut == std::string::npos) ? contenu.size() : saut + 1;
        lignes.push_back(contenu.substr(debut, fin - debut));
        debut = fin;
    }

    /* Clés déjà présentes : si le fichier porte déjà le nom actuel, renommer
       l'ancienne ligne créerait un doublon, et la dernière valeur lue l'emporterait
       sans que personne comprenne pourquoi. */
    std::set<std::string> presentes;
    for (const std::string& ligne : lignes) {
        const std::string cle = cleDeLigne(trim(ligne));
        if (!cle.empty()) {
            presentes.insert(cle);
        }
    }

    const bool crlf = contenu.find("\r\n") != std::string::npos;
    const char* fdl = crlf ? "\r\n" : "\n";

    std::size_t modifiees = 0;
    std::vector<std::string> sortie;
    sortie.reserve(lignes.size() + 4);
    for (const std::string& ligne : lignes) {
        const std::size_t debut = ligne.find_first_not_of(" \t");
        if (debut == std::string::npos || ligne[debut] == '#') {
            sortie.push_back(ligne);
            continue;
        }
        std::size_t fin = ligne.find_first_of(" \t\r\n", debut);
        if (fin == std::string::npos) {
            fin = ligne.size();
        }
        const std::string cle = ligne.substr(debut, fin - debut);
        const auto trouv = renommages.find(cle);
        if (trouv == renommages.end()) {
            sortie.push_back(ligne);
            continue;
        }
        if (presentes.find(trouv->second) != presentes.end()) {
            /* Le nom actuel est déjà là : on neutralise l'ancienne ligne au lieu
               d'en faire un doublon, en expliquant pourquoi juste au-dessus. */
            sortie.push_back(std::string("# Ancien nom de \"") + trouv->second +
                             "\", que ce fichier porte déjà : ligne neutralisée par le "
                             "simulateur." +
                             fdl);
            sortie.push_back("# " + ligne);
        } else {
            /* Renommage : seul le nom change, la valeur et la mise en page restent
               exactement ce que l'utilisateur avait écrit. */
            sortie.push_back(ligne.substr(0, debut) + trouv->second + ligne.substr(fin));
            /* Le nom actuel existe désormais dans le fichier : si deux anciens noms
               menaient au même, le second sera neutralisé, pas dupliqué. */
            presentes.insert(trouv->second);
        }
        ++modifiees;
        std::printf("[Config] option renommée dans %s : %s -> %s\n",
                    config.filename().string().c_str(),
                    cle.c_str(),
                    trouv->second.c_str());
    }
    if (modifiees == 0) {
        return 0; /* rien à changer : on ne touche pas au fichier */
    }

    /* Réécriture par fichier intermédiaire puis remplacement : contrairement au
       complètement, qui n'ajoute qu'à la fin, on réécrit ici tout le fichier. Une
       coupure de courant ou un disque plein au mauvais moment ne doit pas laisser
       l'utilisateur avec une configuration tronquée. */
    const std::filesystem::path temporaire = config.string() + ".nouveau";
    {
        std::ofstream out(temporaire, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::fprintf(stderr,
                         "[Config] %s non modifiable : les options renommées gardent leur ancien "
                         "nom et seront ignorées.\n",
                         config.filename().string().c_str());
            return 0;
        }
        for (const std::string& ligne : sortie) {
            out << ligne;
        }
        if (!out) {
            std::fprintf(stderr,
                         "[Config] écriture de %s incomplète : fichier d'origine gardé.\n",
                         temporaire.filename().string().c_str());
            out.close();
            std::error_code ec;
            std::filesystem::remove(temporaire, ec);
            return 0;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporaire, config, ec);
    if (ec) {
        std::fprintf(stderr,
                     "[Config] remplacement de %s impossible (%s) : fichier d'origine gardé.\n",
                     config.filename().string().c_str(),
                     ec.message().c_str());
        std::filesystem::remove(temporaire, ec);
        return 0;
    }
    return modifiees;
}

} /* namespace artouste::app */
