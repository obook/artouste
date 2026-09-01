/*
 * LigneCommande.cpp
 * Lecture des options de lancement. Analyse volontairement simple : une
 * quinzaine d'options, toutes de la forme --nom valeur, aucune dépendance.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/LigneCommande.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace artouste::app {

namespace {

/* Vrai si l'argument est l'option attendue, sous sa forme longue ou courte. */
bool est(const char* arg, const char* longue, const char* courte = nullptr) {
    return std::strcmp(arg, longue) == 0 ||
           (courte != nullptr && std::strcmp(arg, courte) == 0);
}

/* Équivalent ASCII d'une lettre accentuée latine-1, donnée par le second octet
   de sa séquence UTF-8 (préfixe 0xC3). 0 si l'octet ne code aucune lettre. */
char sansAccent(unsigned char second) {
    if (second >= 0x80 && second <= 0x85) { return 'a'; } /* ÀÁÂÃÄÅ */
    if (second >= 0xA0 && second <= 0xA5) { return 'a'; } /* àáâãäå */
    if (second == 0x87 || second == 0xA7) { return 'c'; } /* Çç */
    if (second >= 0x88 && second <= 0x8B) { return 'e'; } /* ÈÉÊË */
    if (second >= 0xA8 && second <= 0xAB) { return 'e'; } /* èéêë */
    if (second >= 0x8C && second <= 0x8F) { return 'i'; } /* ÌÍÎÏ */
    if (second >= 0xAC && second <= 0xAF) { return 'i'; } /* ìíîï */
    if (second == 0x91 || second == 0xB1) { return 'n'; } /* Ññ */
    if (second >= 0x92 && second <= 0x96) { return 'o'; } /* ÒÓÔÕÖ */
    if (second >= 0xB2 && second <= 0xB6) { return 'o'; } /* òóôõö */
    if (second >= 0x99 && second <= 0x9C) { return 'u'; } /* ÙÚÛÜ */
    if (second >= 0xB9 && second <= 0xBC) { return 'u'; } /* ùúûü */
    if (second == 0x9D || second == 0xBD || second == 0xBF) { return 'y'; } /* Ýýÿ */
    return '\0';
}

} /* namespace */

std::string normaliserNom(const std::string& nom) {
    std::string out;
    out.reserve(nom.size());
    for (std::size_t i = 0; i < nom.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(nom[i]);
        if (c >= 'A' && c <= 'Z') {
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out.push_back(static_cast<char>(c));
        } else if (c == 0xC3 && i + 1 < nom.size()) {
            /* Lettre accentuée latine-1 en UTF-8 : deux octets, 0xC3 puis la
               lettre. On consomme le second dans tous les cas, sinon il
               ressortirait seul et polluerait la comparaison. */
            const char ascii = sansAccent(static_cast<unsigned char>(nom[++i]));
            if (ascii != '\0') {
                out.push_back(ascii);
            }
        } else if (c == 0xC5 && i + 1 < nom.size()) {
            /* Œ et œ (U+0152 / U+0153), hors latine-1 : "coeur" s'écrit parfois
               avec la ligature. */
            const unsigned char second = static_cast<unsigned char>(nom[++i]);
            if (second == 0x92 || second == 0x93) {
                out += "oe";
            }
        } else if (c >= 0xC0) {
            /* Début d'une autre séquence multi-octets : on la saute entière
               plutôt que d'en garder des morceaux. */
            std::size_t suite = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : 1;
            i += suite;
        }
        /* Tout le reste (espaces, tirets, apostrophes, ponctuation) est ignoré :
           "Notre-Dame de Paris" se cherche aussi bien en tapant "notredame". */
    }
    return out;
}

OptionsLancement lireLigneCommande(int argc, char** argv) {
    OptionsLancement o;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];

        if (est(a, "--aide", "-h") || est(a, "--help")) {
            o.aide = true;
            return o;
        }

        if (est(a, "--gpu")) {
            o.gpu = true;
            return o;
        }

        /* Toutes les autres options attendent une valeur. On le vérifie une fois
           ici plutôt qu'à chaque branche : une option en fin de ligne sans sa
           valeur est une erreur, pas une option booléenne. */
        const bool aValeur = (i + 1 < argc);
        auto valeur = [&]() -> const char* { return argv[++i]; };

        if (est(a, "--carte", "-c")) {
            if (!aValeur) { o.erreur = true; break; }
            o.carte = valeur();
        } else if (est(a, "--monument", "-m")) {
            if (!aValeur) { o.erreur = true; break; }
            o.monument = valeur();
        } else if (est(a, "--lieu", "-l")) {
            if (!aValeur) { o.erreur = true; break; }
            o.lieu = valeur();
        } else if (est(a, "--lon")) {
            if (!aValeur) { o.erreur = true; break; }
            o.lon = std::strtof(valeur(), nullptr);
            o.aLonLat = true;
        } else if (est(a, "--lat")) {
            if (!aValeur) { o.erreur = true; break; }
            o.lat = std::strtof(valeur(), nullptr);
            o.aLonLat = true;
        } else if (est(a, "--alt", "-a")) {
            if (!aValeur) { o.erreur = true; break; }
            o.altitude  = std::strtof(valeur(), nullptr);
            o.aAltitude = true;
        } else if (est(a, "--cap")) {
            if (!aValeur) { o.erreur = true; break; }
            o.cap  = std::strtof(valeur(), nullptr);
            o.aCap = true;
        } else {
            std::fprintf(stderr, "Option inconnue : %s\n", a);
            o.erreur = true;
            break;
        }
    }

    /* --lon sans --lat (ou l'inverse) ne désigne aucun point : le drapeau est
       posé par l'une comme par l'autre, on le retire si les deux ne sont pas là.
       Sans ce contrôle, l'appareil apparaîtrait sur le méridien de Greenwich. */
    if (o.aLonLat && (o.lon == 0.0f || o.lat == 0.0f)) {
        std::fprintf(stderr, "--lon et --lat vont par paire.\n");
        o.erreur = true;
    }

    return o;
}

void afficherAide(const char* programme) {
    std::printf(
        "Artouste, simulateur de vol en hélicoptère.\n"
        "\n"
        "Usage : %s [options]\n"
        "\n"
        "Sans option, le menu de démarrage s'ouvre et le vol commence au pad de\n"
        "la carte. Les options ci-dessous servent à reprendre un vol directement\n"
        "à l'endroit voulu, sans convoyage.\n"
        "\n"
        "  -c, --carte NOM       Carte à charger (dossier sous assets/terrain/),\n"
        "                        par exemple paris, arcachon, bigorre. Saute le\n"
        "                        menu de démarrage.\n"
        "  -m, --monument NOM    Apparaître au-dessus de ce monument, cherché par\n"
        "                        son nom dans le monuments.txt de la carte.\n"
        "  -l, --lieu NOM        Idem, mais cherché dans landmarks.txt.\n"
        "      --lon DEG         Position WGS84 explicite, longitude et latitude\n"
        "      --lat DEG         en degrés. Les deux vont ensemble.\n"
        "  -a, --alt METRES      Hauteur AU-DESSUS DU SOL, pas altitude absolue.\n"
        "                        0 pose l'appareil sur le relief. Défaut : 300.\n"
        "      --cap DEG         Cap boussole (0 = nord, 90 = est). Sans lui, le\n"
        "                        cap de départ de la carte.\n"
        "      --gpu             Lister les cartes graphiques de la machine et\n"
        "                        sortir (Linux uniquement). Utile sur un\n"
        "                        portable à deux cartes.\n"
        "  -h, --aide            Cette aide.\n"
        "\n"
        "Dès qu'un point d'apparition est demandé, la turbine et le rotor sont mis\n"
        "au régime : naître en vol moteur arrêté, c'est tomber.\n"
        "\n"
        "Le nom cherché par --monument et --lieu l'est sans tenir compte de la\n"
        "casse ni des accents, et un fragment suffit : \"pantheon\" trouve\n"
        "\"Panthéon\".\n"
        "\n"
        "Exemples :\n"
        "  %s --carte paris --monument \"Pantheon\" --alt 300\n"
        "  %s --carte paris --lieu \"Tour Eiffel\" --alt 200 --cap 270\n"
        "  %s --carte paris --lon 2.346073 --lat 48.846167 --alt 300\n"
        "\n"
        "Les variables d'environnement ARTOUSTE_* restent lues et gardent la\n"
        "priorité sur les options équivalentes.\n",
        programme, programme, programme, programme);
}

} /* namespace artouste::app */
