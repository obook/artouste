/*
 * CartesGraphiques.cpp
 * Énumération des cartes graphiques par le noyau, sans dépendance externe :
 * /sys/class/drm donne le pilote et les identifiants PCI, /usr/share/misc/pci.ids
 * les traduit en noms lisibles. Cette base n'est pas toujours installée, auquel
 * cas les identifiants bruts sont affichés tels quels.
 *
 * Ce que le noyau expose n'est pas ce qu'OpenGL sait piloter : une carte dont la
 * partie utilisateur du pilote manque apparaîtra quand même ici. La carte
 * réellement employée n'est connue qu'une fois le contexte créé, elle est
 * affichée au démarrage par ApplicationLifecycle.
 *
 * Auteur : O. Booklage
 * Date : septembre 2026
 * Licence : GPL v2
 */

#include "app/CartesGraphiques.hpp"

#include <cstdio>
#include <string>

#ifdef __linux__
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#endif

namespace artouste::app {

#ifdef __linux__

namespace {

/* Base de noms des matériels PCI, fournie par le paquet hwdata. */
constexpr char BASE_PCI[] = "/usr/share/misc/pci.ids";

/* Une carte graphique telle que la décrit le noyau. */
struct Carte {
    std::string noeud;     /* /dev/dri/renderD128 */
    std::string pilote;    /* i915, amdgpu, nouveau, nvidia */
    std::string idVendeur; /* 8086 */
    std::string idModele;  /* 3e9b */
};

std::string enMinuscules(std::string texte) {
    for (char& c : texte) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return texte;
}

/* Lit la valeur d'une clé dans un fichier uevent, dont chaque ligne vaut
   "CLE=valeur". Renvoie une chaîne vide si la clé est absente. */
std::string lireCle(const std::filesystem::path& fichier, const std::string& cle) {
    std::ifstream     flux(fichier);
    const std::string prefixe = cle + "=";
    std::string       ligne;
    while (std::getline(flux, ligne)) {
        if (ligne.rfind(prefixe, 0) == 0) {
            return ligne.substr(prefixe.size());
        }
    }
    return {};
}

/*
Cherche les noms lisibles du vendeur et du modèle dans pci.ids. Le fichier est
indenté : une ligne sans tabulation ouvre un vendeur, une ligne précédée d'une
tabulation décrit un de ses modèles, deux tabulations un sous-système que l'on
ignore. Les noms commencent après l'identifiant et deux espaces.
*/
void resoudreNoms(const Carte& carte, std::string& nomVendeur, std::string& nomModele) {
    std::ifstream flux(BASE_PCI);
    if (!flux) {
        return;
    }

    bool        dansVendeur = false;
    std::string ligne;
    while (std::getline(flux, ligne)) {
        if (ligne.empty() || ligne[0] == '#') {
            continue;
        }

        if (ligne[0] != '\t') {
            /* Le vendeur suivant commence : le nôtre est passé sans que son
               modèle soit trouvé, inutile de lire les 1,4 Mo restants. */
            if (dansVendeur) {
                return;
            }
            if (ligne.rfind(carte.idVendeur, 0) == 0 && ligne.size() > 6) {
                dansVendeur = true;
                nomVendeur  = ligne.substr(6);
            }
            continue;
        }

        if (!dansVendeur || (ligne.size() > 1 && ligne[1] == '\t')) {
            continue;
        }
        if (ligne.size() > 7 && ligne.compare(1, 4, carte.idModele) == 0) {
            nomModele = ligne.substr(7);
            return;
        }
    }
}

/* Un noeud de rendu par carte utilisable : c'est l'énumération la plus fiable,
   les noeuds "card" comptant aussi les sorties vidéo. */
std::vector<Carte> detecterCartes() {
    std::vector<Carte>          cartes;
    const std::filesystem::path racine = "/sys/class/drm";
    std::error_code             erreur;
    if (!std::filesystem::exists(racine, erreur)) {
        return cartes;
    }

    for (const auto& entree : std::filesystem::directory_iterator(racine, erreur)) {
        const std::string nom = entree.path().filename().string();
        if (nom.rfind("renderD", 0) != 0) {
            continue;
        }

        /* PCI_ID vaut "8086:3E9B" : quatre chiffres, deux-points, quatre chiffres. */
        const std::string pciId = lireCle(entree.path() / "device" / "uevent", "PCI_ID");
        if (pciId.size() != 9 || pciId[4] != ':') {
            continue;
        }

        Carte carte;
        carte.noeud     = "/dev/dri/" + nom;
        carte.pilote    = lireCle(entree.path() / "device" / "uevent", "DRIVER");
        carte.idVendeur = enMinuscules(pciId.substr(0, 4));
        carte.idModele  = enMinuscules(pciId.substr(5, 4));
        cartes.push_back(carte);
    }

    std::sort(cartes.begin(), cartes.end(), [](const Carte& a, const Carte& b) {
        return a.noeud < b.noeud;
    });
    return cartes;
}

}  /* namespace */

void afficherCartesGraphiques() {
    const std::vector<Carte> cartes = detecterCartes();
    if (cartes.empty()) {
        std::printf("Aucune carte graphique détectée dans /sys/class/drm.\n");
        return;
    }

    std::printf("Cartes graphiques détectées :\n\n");
    for (const Carte& carte : cartes) {
        std::string nomVendeur;
        std::string nomModele;
        resoudreNoms(carte, nomVendeur, nomModele);
        if (nomVendeur.empty()) {
            nomVendeur = carte.idVendeur;
        }
        if (nomModele.empty()) {
            nomModele = carte.idModele;
        }

        std::printf("  %s - %s\n", nomVendeur.c_str(), nomModele.c_str());
        std::printf("      pilote %s, PCI %s:%s, %s\n\n",
                    carte.pilote.c_str(),
                    carte.idVendeur.c_str(),
                    carte.idModele.c_str(),
                    carte.noeud.c_str());
    }

    if (cartes.size() > 1) {
        std::printf("Lancer sur la carte dédiée : ./play-linux.sh\n");
    }
    std::printf("La carte réellement utilisée est affichée au démarrage, "
                "ligne \"Renderer\".\n");
}

#else

void afficherCartesGraphiques() {
    /* Énumérer les cartes sous Windows demanderait DXGI ou WMI. En attendant, on
       renvoie à la seule information disponible, celle que le pilote donne au
       démarrage, plutôt que de laisser l'utilisateur sans réponse. */
    std::printf("L'énumération des cartes graphiques n'est disponible que sous Linux.\n");
    std::printf("La carte réellement utilisée est affichée au démarrage, "
                "ligne \"Renderer\".\n");
}

#endif

const char* nomCourtGpu(const char* renderer) {
    if (renderer == nullptr) {
        return "?";
    }
    const std::string nom = renderer;
    /* nouveau et NVK sont les pilotes libres des cartes NVIDIA : même matériel,
       donc même nom affiché. */
    if (nom.find("NVIDIA") != std::string::npos || nom.find("nouveau") != std::string::npos
        || nom.find("NVK") != std::string::npos) {
        return "NVIDIA";
    }
    if (nom.find("Intel") != std::string::npos) {
        return "INTEL";
    }
    if (nom.find("AMD") != std::string::npos || nom.find("Radeon") != std::string::npos) {
        return "AMD";
    }
    /* Rendu par le processeur, faute de pilote matériel : quelques images par
       seconde au mieux. */
    if (nom.find("llvmpipe") != std::string::npos
        || nom.find("softpipe") != std::string::npos) {
        return "LOGICIEL";
    }
    return "AUTRE";
}

bool renduSurCarteIntegree(const char* renderer) {
#ifdef __linux__
    if (renderer == nullptr) {
        return false;
    }
    /* Un second noeud de rendu signale une seconde carte : sans lui, il n'y a
       rien vers quoi basculer et l'information n'aurait aucune utilité. */
    std::error_code erreur;
    if (!std::filesystem::exists("/dev/dri/renderD129", erreur)) {
        return false;
    }
    const std::string nom = renderer;
    return nom.find("Intel") != std::string::npos
           || nom.find("llvmpipe") != std::string::npos;
#else
    (void) renderer;
    return false;
#endif
}

}  /* namespace artouste::app */
