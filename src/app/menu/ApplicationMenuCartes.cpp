/*
 * ApplicationMenuCartes.cpp
 * Gestionnaire de cartes : l'écran qui montre ce que chaque carte occupe sur le
 * disque et laisse en disposer, ouvert depuis le menu de démarrage.
 *
 * Une carte n'est plus un bloc mais un empilement (voir docs/DISTRIBUTION.md) :
 * un socle léger toujours présent, des bâtiments facultatifs, et des tuiles
 * d'orthophoto fine qui pèsent mille fois plus que le reste. Sans un endroit
 * pour voir et défaire tout cela, un simulateur de 45 Mo finirait par occuper
 * plusieurs gigaoctets sans que personne sache lesquels ni pourquoi.
 *
 * D'où la règle qui gouverne cet écran : ANNONCER AVANT D'AGIR. Chaque ligne
 * porte sa taille, l'espace libre du disque est affiché en permanence, et rien
 * de destructeur ne part sans confirmation.
 *
 * Les arbres et les bâtiments se cochent ici aussi, et par carte : les arbres
 * comptent en montagne, les bâtiments en ville, rarement les deux au même
 * endroit. Tous deux s'affichent en oui ou non, et non en mégaoctets : les
 * éteindre ne rend pas un octet, seul le retrait de la carte le fait. Leur poids
 * est donc compté dans le socle. Le choix est écrit dans le options.txt de la
 * carte, que le moteur relit au chargement (voir ApplicationScene.cpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "app/Application.hpp"

#include "app/cartes/FabriqueTuiles.hpp"
#include "input/Keyboard.hpp"
#include "render/tuiles/Pyramide.hpp"
#include "ui/HudWidgets.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace artouste::app {

/* La variable locale de l'écran s'appelle aussi cartes : on nomme donc le
   sous-système de fabrication autrement, plutôt que de compter sur la règle de
   résolution qui distingue les deux. */
namespace fab = artouste::app::cartes;

namespace {

/* Taille cumulée d'un dossier, sous-dossiers compris. Un jeu de tuiles compte
   des milliers de fichiers : on ne la recalcule donc qu'à l'ouverture de
   l'écran et sur demande, jamais à chaque image. */
[[nodiscard]] std::uintmax_t tailleDossier(const std::filesystem::path& dossier,
                                           int* tuiles = nullptr) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dossier, ec)) {
        return 0;
    }
    std::uintmax_t total = 0;
    for (const auto& entree :
         std::filesystem::recursive_directory_iterator(dossier, ec)) {
        if (entree.is_regular_file(ec)) {
            total += entree.file_size(ec);
            /* Les tuiles sont comptées au passage : la marche est déjà faite, et
               les compter à part rouvrirait des milliers de fichiers. */
            if (tuiles != nullptr && entree.path().extension() == ".dds") {
                ++*tuiles;
            }
        }
    }
    return total;
}

[[nodiscard]] std::uintmax_t tailleFichier(const std::filesystem::path& fichier) {
    std::error_code ec;
    const auto      taille = std::filesystem::file_size(fichier, ec);
    return ec ? 0 : taille;
}

/* Taille lisible : on ne montre jamais des octets à quelqu'un qui décide de
   remplir son disque. */
[[nodiscard]] std::string formaterOctets(std::uintmax_t octets) {
    char tampon[32];
    if (octets >= 1000ull * 1000ull * 1000ull) {
        std::snprintf(tampon, sizeof(tampon), "%.1f Go", static_cast<double>(octets) / 1e9);
    } else if (octets >= 1000ull * 1000ull) {
        std::snprintf(tampon, sizeof(tampon), "%.0f Mo", static_cast<double>(octets) / 1e6);
    } else if (octets > 0) {
        std::snprintf(tampon, sizeof(tampon), "%.0f Ko", static_cast<double>(octets) / 1e3);
    } else {
        std::snprintf(tampon, sizeof(tampon), "-");
    }
    return tampon;
}

/* Débit lisible. Le même vocabulaire que les tailles, mais la marche du
   mégaoctet est trop haute pour un débit : une ligne à 205 Ko/s s'affichait
   "0,2 Mo/s", où la moitié des chiffres utiles ont disparu. On descend donc en
   kilooctets sous le mégaoctet, et on garde une décimale au-dessus. */
[[nodiscard]] std::string formaterDebit(double octetsParSeconde) {
    char tampon[32];
    if (octetsParSeconde >= 1e6) {
        std::snprintf(tampon, sizeof(tampon), "%.1f Mo/s", octetsParSeconde / 1e6);
    } else {
        std::snprintf(tampon, sizeof(tampon), "%.0f Ko/s", octetsParSeconde / 1e3);
    }
    return tampon;
}

/* Tourniquet d'attente : le caractère tourne tant que la fabrication travaille,
   et dit d'un coup d'oeil que le programme n'est pas figé, là où un débit et une
   durée qui ne bougent qu'à la fin d'un bloc laissent le doute.

   Quatre positions ASCII et pas un caractère plus joli : la police par défaut
   d'ImGui, la seule chargée ici, s'arrête au latin-1. Un braille tournant ou un
   demi-bloc n'y donnerait qu'un carré vide. Huit changements par seconde, la
   cadence à laquelle l'oeil lit un mouvement sans être agacé. */
[[nodiscard]] char caractereTournant(double secondes) {
    static constexpr char PHASES[] = {'|', '/', '-', '\\'};
    const int             phase    = static_cast<int>(secondes * 8.0) % 4;
    return PHASES[phase < 0 ? 0 : phase];
}

/* Durée restante lisible : en heures dès la soixantième minute, comme l'annonce
   d'avant lancement. Au-delà de l'heure on ne compte plus en minutes, "112
   minutes" se compte mal. L'arrondi précède le choix de l'unité, sans quoi 59,7
   minutes s'afficheraient "60 minutes". */
[[nodiscard]] std::string formaterDuree(double secondes) {
    char         tampon[32];
    const double minutes = std::round(secondes / 60.0);
    if (minutes >= 60.0) {
        std::snprintf(tampon, sizeof(tampon), "%.0f h %02.0f", std::floor(minutes / 60.0),
                      minutes - 60.0 * std::floor(minutes / 60.0));
    } else {
        std::snprintf(tampon, sizeof(tampon), "%.0f minutes", minutes);
    }
    return tampon;
}

}  /* namespace */

std::vector<Application::EtatCarte>
Application::inventorierCartes(const std::filesystem::path& assets) {
    std::vector<EtatCarte> etats;
    for (const MapEntry& carte : recenserCartes(assets)) {
        const std::filesystem::path dossier = assets / "terrain" / carte.dir;

        EtatCarte etat;
        etat.dir              = carte.dir;
        etat.titre            = carte.title;
        etat.dossier          = dossier;
        etat.octetsBatiments  = tailleFichier(dossier / "buildings.bin");
        etat.dossierTuiles    = render::tuiles::cheminJeuDeTuiles(dossier, racineTuiles());
        etat.octetsTuiles     = tailleDossier(etat.dossierTuiles, &etat.tuilesPresentes);
        /* Finesse du jeu en place : celle de son niveau le plus fin, les niveaux
           revenant classés du plus large au plus fin. Ne lit que les index, pas
           les tuiles. */
        if (!etat.dossierTuiles.empty()) {
            const auto niveaux = render::tuiles::ouvrirNiveaux(etat.dossierTuiles);
            if (!niveaux.empty()) {
                etat.finesseTuiles   = niveaux.back().calage().mParPixel;
                etat.tuilesAttendues = niveaux.back().calage().colonnes *
                                       niveaux.back().calage().rangees;
            }
            etat.tuilesInachevees = fab::fabricationInachevee(etat.dossierTuiles);
        }
        etat.interet = fab::interet(dossier);
        /* Le socle, c'est tout ce que porte le dossier de la carte, moins les
           seules tuiles quand elles y sont rangées. Les bâtiments y sont compris :
           l'écran ne sait pas les supprimer à part, et les sortir du compte
           laissait des mégaoctets invisibles sur une ligne dont c'est le sujet. */
        const std::uintmax_t brut = tailleDossier(dossier);
        const std::uintmax_t tuilesDedans =
            (etat.dossierTuiles.empty() || etat.dossierTuiles.parent_path() != dossier)
                ? 0
                : etat.octetsTuiles;
        etat.octetsSocle = brut - std::min(brut, tuilesDedans);

        /* Options effectives : celles de la carte si elle en a, sinon la
           configuration générale. On lit m_config et non m_treesEnabled : ce
           dernier n'est calculé qu'au chargement de la scène, alors que cet écran
           s'ouvre depuis le menu, avant. */
        etat.arbres    = m_config.trees && std::getenv("ARTOUSTE_NO_TREES") == nullptr;
        etat.batiments = true;
        std::ifstream options(dossier / "options.txt");
        std::string   cle, valeur;
        while (options >> cle) {
            if (!cle.empty() && cle[0] == '#') {
                std::getline(options, cle);
                continue;
            }
            if (!(options >> valeur)) {
                break;
            }
            const bool oui = !(valeur == "0" || valeur == "non" || valeur == "false");
            if (cle == "arbres") {
                etat.arbres       = oui;
                etat.arbresDefini = true;
            } else if (cle == "batiments") {
                etat.batiments       = oui;
                etat.batimentsDefini = true;
            } else if (cle == "tuiles") {
                etat.tuiles        = oui;
                etat.tuilesDefinie = true;
            }
        }
        etats.push_back(std::move(etat));
    }
    return etats;
}

void Application::runGestionnaireCartes() {
    /* Curseur visible le temps de cet écran, masqué en sortant : mêmes raisons que
       dans runStartupMenu, qui commente le partage. */
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    struct RemasquerEnSortant {
        GLFWwindow* fenetre;
        ~RemasquerEnSortant() { glfwSetInputMode(fenetre, GLFW_CURSOR, GLFW_CURSOR_HIDDEN); }
    } remasquer{m_window};

    /* Cet écran s'ouvre depuis le menu de démarrage, donc AVANT le chargement de
       la scène : m_assetsDir n'est pas encore renseigné à ce moment-là, et il
       faut localiser les ressources soi-même, comme le fait runStartupMenu. */
    const std::filesystem::path assets = m_assetsDir.empty() ? resolveAssetDir() : m_assetsDir;

    std::vector<EtatCarte> cartes = inventorierCartes(assets);
    if (cartes.empty()) {
        std::fprintf(stderr, "[cartes] aucune carte trouvée dans %s\n",
                     (assets / "terrain").string().c_str());
        return;
    }

    std::size_t selection  = 0;
    int         aSupprimer = -1;  /* carte dont la suppression de tuiles est à confirmer */
    int         aFabriquer = -1;  /* carte dont la fabrication est à confirmer */
    bool        fini       = false;
    int         images     = 0;

    /* Fabrication des tuiles : elle tourne dans son propre fil, l'écran ne fait
       que la suivre. La finesse visée n'est PAS la même pour toutes les cartes :
       elle se déduit de l'orthophoto d'ensemble de chacune (fab::interet), faute
       de quoi une petite carte déjà fine se verrait proposer des tuiles qui ne
       changeraient rien à ce qu'on voit. */
    fab::Fabrique   fabrique;
    fab::Estimation estimation;

    /* Des tuiles ne comptent que si elles sont plus fines que l'orthophoto
       d'ensemble : le moteur écarte les autres au chargement, et la carte reste
       en LR malgré les mégaoctets posés sur le disque. Une carte dont on n'a pas
       su mesurer l'orthophoto garde le bénéfice du doute. */
    const auto tuilesEfficaces = [](const EtatCarte& c) {
        return c.octetsTuiles > 0 &&
               (c.interet.ortho <= 0.0f || c.finesseTuiles <= 0.0f ||
                c.interet.ortho >= c.finesseTuiles * fab::GAIN_MINIMUM);
    };

    /* Les deux actions lourdes sont écrites une seule fois : le clavier et les
       boutons de souris passent par elles, sans quoi les deux chemins finiraient
       par diverger. */
    /* Destination des tuiles : celle qui existe déjà, sinon celle où le moteur
       ira les chercher (ARTOUSTE_TUILES, ou la carte elle-même). Calculée par une
       seule fonction, car l'annonce doit nommer EXACTEMENT le dossier que le
       lancement va remplir. */
    const auto destinationTuiles = [this](const EtatCarte& c) {
        if (!c.dossierTuiles.empty()) {
            return c.dossierTuiles;
        }
        const std::filesystem::path racine = racineTuiles();
        return racine.empty() ? c.dossier / "tuiles" : racine / c.dir;
    };
    /* Finesse à demander : celle du jeu déjà entamé s'il y en a un, sinon celle que
       vise la carte. Reprendre à une autre finesse réécrirait l'index et laisserait
       deux grilles incompatibles dans le même dossier. */
    const auto finesseAFabriquer = [](const EtatCarte& c) {
        return (c.tuilesInachevees && c.finesseTuiles > 0.0f) ? c.finesseTuiles
                                                              : c.interet.visee;
    };
    /* Les deux actions qui touchent au disque lèvent le même drapeau : la scène
       peut être en mémoire, avec sa fenêtre de détail construite au chargement, et
       elle doit être rechargée au prochain lancement. On le lève dès le DÉPART de
       la fabrication et non à son terme : une fabrication arrêtée en route a tout
       de même posé ses premières tuiles. */
    const auto lancerFabrication = [this, &fabrique, &destinationTuiles,
                                    &finesseAFabriquer](EtatCarte& c) {
        fabrique.lancer(c.dossier, destinationTuiles(c), finesseAFabriquer(c));
        m_cartesRemaniees = true;
    };
    /* N'écrit que les réglages explicitement pris pour cette carte : les autres
       restent absents du fichier et continuent donc de suivre la configuration
       générale. */
    const auto ecrireOptions = [](const EtatCarte& c) {
        if (!c.arbresDefini && !c.batimentsDefini && !c.tuilesDefinie) {
            std::error_code ec;
            std::filesystem::remove(c.dossier / "options.txt", ec);
            return;
        }
        std::ofstream out(c.dossier / "options.txt", std::ios::trunc);
        if (!out) {
            return;
        }
        out << "# Options de la carte, écrites par le gestionnaire de cartes.\n";
        out << "# Le moteur les relit au chargement ; une clé absente rend la main à\n";
        out << "# la configuration générale (assets/config.txt).\n";
        if (c.arbresDefini) {
            out << "arbres " << (c.arbres ? 1 : 0) << "\n";
        }
        if (c.batimentsDefini) {
            out << "batiments " << (c.batiments ? 1 : 0) << "\n";
        }
        if (c.tuilesDefinie) {
            out << "tuiles " << (c.tuiles ? 1 : 0) << "\n";
        }
    };

    /* Ce que vaut un réglage qu'aucune carte n'a pris pour elle : même calcul que
       dans inventorierCartes, dont il doit rester le reflet exact. */
    const bool arbresGeneral = m_config.trees && std::getenv("ARTOUSTE_NO_TREES") == nullptr;

    /* Rend ses trois réglages à la configuration générale : le fichier de la
       carte disparaît, et elle suivra de nouveau tout changement commun. Sans
       cela, un réglage pris une fois l'était pour toujours. */
    const auto rendreAuDefaut = [arbresGeneral](EtatCarte& c) {
        std::error_code ec;
        std::filesystem::remove(c.dossier / "options.txt", ec);
        c.arbres          = arbresGeneral;
        c.arbresDefini    = false;
        c.batiments       = true;
        c.batimentsDefini = false;
        c.tuiles          = true;
        c.tuilesDefinie   = false;
    };

    const auto supprimerTuiles = [this](EtatCarte& c) {
        std::error_code effacement;
        std::filesystem::remove_all(c.dossierTuiles, effacement);
        c.octetsTuiles      = 0;
        c.tuilesPresentes   = 0;
        c.tuilesInachevees  = false;
        c.finesseTuiles     = 0.0f;
        c.dossierTuiles.clear();
        m_cartesRemaniees = true;
    };

    bool pvHaut = false, pvBas = false, pvRetour = false, pvArbres = false, pvBatiments = false,
         pvTuiles = false, pvValider = false, pvSupprimer = false, pvRendre = false;
    const auto edge = [](bool actuel, bool& precedent) {
        const bool front = actuel && !precedent;
        precedent        = actuel;
        return front;
    };

    while (!fini && glfwWindowShouldClose(m_window) == 0) {
        glfwPollEvents();

        /* Le curseur est masqué en plein écran : TOUT doit être atteignable au
           clavier, les boutons n'étant qu'un raccourci pour qui a la souris.

           Entrée est la touche d'action, comme au menu de démarrage où elle
           démarre le vol : ici elle lance la fabrication, puis confirme. Échap
           annule ce qui est en cours avant de fermer l'écran. La touche F n'est
           pas disponible pour "fabriquer" : elle bascule le plein écran partout
           dans le jeu, menu compris. */
        const bool haut   = glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS;
        const bool bas    = glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS;
        const bool retour = glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        const bool valider = glfwGetKey(m_window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                             glfwGetKey(m_window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
        const bool supprimer     = glfwGetKey(m_window, GLFW_KEY_DELETE) == GLFW_PRESS;
        /* Les lettres passent par la disposition réelle du clavier : le jeton
           GLFW_KEY_A désigne une POSITION, celle qui écrit "q" en AZERTY. */
        const bool bascArbres    = glfwGetKey(m_window, input::toucheImprimant('a')) == GLFW_PRESS;
        const bool bascBatiments = glfwGetKey(m_window, input::toucheImprimant('b')) == GLFW_PRESS;
        const bool bascTuiles    = glfwGetKey(m_window, input::toucheImprimant('t')) == GLFW_PRESS;
        const bool rendre        = glfwGetKey(m_window, input::toucheImprimant('r')) == GLFW_PRESS;

        const bool frontRetour    = edge(retour, pvRetour);
        const bool frontValider   = edge(valider, pvValider);
        const bool frontSupprimer = edge(supprimer, pvSupprimer);
        const bool frontArbres    = edge(bascArbres, pvArbres);
        const bool frontBatiments = edge(bascBatiments, pvBatiments);
        const bool frontTuiles    = edge(bascTuiles, pvTuiles);
        const bool frontRendre    = edge(rendre, pvRendre);
        const bool frontHaut      = edge(haut, pvHaut);
        const bool frontBas       = edge(bas, pvBas);

        EtatCarte& courante = cartes[selection];
        if (fabrique.enCours()) {
            /* Pendant la fabrication, la seule décision possible est de l'arrêter. */
            if (frontRetour) {
                fabrique.annuler();
            }
        } else if (fabrique.avancement().termine) {
            /* Compte rendu affiché : n'importe laquelle des deux touches le ferme,
               et l'inventaire est refait puisque le disque a changé. */
            if (frontRetour || frontValider) {
                fabrique.oublier();
                cartes    = inventorierCartes(assets);
                selection = std::min(selection, cartes.size() - 1);
            }
        } else if (aFabriquer >= 0 || aSupprimer >= 0) {
            /* Une confirmation attend : Entrée confirme, Échap renonce. */
            if (frontValider && aFabriquer >= 0) {
                lancerFabrication(cartes[selection]);
                aFabriquer = -1;
            } else if (frontValider && aSupprimer >= 0) {
                supprimerTuiles(cartes[selection]);
                aSupprimer = -1;
            } else if (frontRetour) {
                aFabriquer = -1;
                aSupprimer = -1;
            }
        } else {
            if (frontHaut && selection > 0) {
                --selection;
            }
            if (frontBas && selection + 1 < cartes.size()) {
                ++selection;
            }
            if (frontRetour) {
                fini = true;
            }
            /* Entrée : fabriquer les tuiles de la carte choisie. On ouvre
               l'annonce, jamais le téléchargement directement. Une carte qui n'a
               rien à y gagner ne s'ouvre pas : ce serait proposer d'occuper le
               disque pour une image identique. */
            if (frontValider && fab::reseauDisponible() &&
                (courante.interet.vaut || courante.tuilesInachevees)) {
                estimation = fab::estimer(courante.dossier, finesseAFabriquer(courante));
                aFabriquer = static_cast<int>(selection);
            }
            if (frontSupprimer && courante.octetsTuiles > 0) {
                aSupprimer = static_cast<int>(selection);
            }
            if (frontArbres) {
                courante.arbres       = !courante.arbres;
                courante.arbresDefini = true;
                ecrireOptions(courante);
            }
            if (frontBatiments) {
                courante.batiments       = !courante.batiments;
                courante.batimentsDefini = true;
                ecrireOptions(courante);
            }
            if (frontRendre) {
                rendreAuDefaut(courante);
            }
            /* Éteindre les tuiles n'a de sens que si la carte en a : sinon la
               touche écrirait une option sans effet, et l'écran afficherait un
               état que rien ne justifie. */
            if (frontTuiles && courante.octetsTuiles > 0) {
                courante.tuiles        = !courante.tuiles;
                courante.tuilesDefinie = true;
                ecrireOptions(courante);
            }
        }

        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(m_window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.09f, 0.11f, 0.13f, 1.0f); /* même fond que le menu */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_hud.updateScale(fbw, fbh);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImVec2 centre(ImGui::GetIO().DisplaySize.x * 0.5f,
                            ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        /* Largeur figée, hauteur libre : sans cela la fenêtre s'ajuste au texte le
           plus long et bondit d'une carte à l'autre, au gré de la longueur des
           titres et des chemins. Les lignes variables passent à la ligne plus
           bas plutôt que d'élargir l'écran. */
        const float largeur = ui::hud_widgets::sc(780.0f);
        ImGui::SetNextWindowSizeConstraints(ImVec2(largeur, 0.0f),
                                            ImVec2(largeur, ImGui::GetIO().DisplaySize.y));
        ImGui::Begin("Artouste -- cartes",
                     nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings);

        /* Socle et tuiles, et rien d'autre : les bâtiments sont désormais compris
           dans le socle, les ajouter les compterait deux fois. */
        std::uintmax_t total = 0;
        for (const EtatCarte& c : cartes) {
            total += c.octetsSocle + c.octetsTuiles;
        }
        ImGui::Text("Cartes installées : %s au total", formaterOctets(total).c_str());

        /* Espace libre de CHAQUE disque concerné : les tuiles vivent souvent
           ailleurs que le jeu (ARTOUSTE_TUILES), et n'annoncer qu'un seul chiffre
           laisserait croire qu'un jeu de tuiles de deux gigaoctets a été pris sur
           le disque système. */
        std::error_code ec;
        const auto      place = std::filesystem::space(assets, ec);
        if (!ec) {
            ImGui::SameLine();
            ImGui::TextDisabled("   jeu : %s libres", formaterOctets(place.available).c_str());
        }
        std::filesystem::path racineTuiles;
        for (const EtatCarte& c : cartes) {
            if (!c.dossierTuiles.empty() && c.dossierTuiles.parent_path() != c.dossier) {
                racineTuiles = c.dossierTuiles.parent_path();
                break;
            }
        }
        if (!racineTuiles.empty()) {
            std::error_code ecTuiles;
            const auto      placeTuiles = std::filesystem::space(racineTuiles, ecTuiles);
            if (!ecTuiles && placeTuiles.available != place.available) {
                ImGui::SameLine();
                ImGui::TextDisabled("   tuiles (%s) : %s libres",
                                    racineTuiles.filename().string().c_str(),
                                    formaterOctets(placeTuiles.available).c_str());
            }
        }
        ImGui::Separator();

        if (ImGui::BeginTable("cartes", 6,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Carte");
            ImGui::TableSetupColumn("Résolution");
            ImGui::TableSetupColumn("Socle");
            ImGui::TableSetupColumn("Tuiles");
            ImGui::TableSetupColumn("Bâtiments");
            ImGui::TableSetupColumn("Arbres");
            ImGui::TableHeadersRow();

            for (std::size_t i = 0; i < cartes.size(); ++i) {
                const EtatCarte& c = cartes[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                /* La ligne en cours est celle que pilotent les touches. */
                if (i == selection) {
                    ImGui::TextUnformatted(">");
                    ImGui::SameLine();
                }
                ImGui::TextUnformatted(c.dir.c_str());

                ImGui::TableNextColumn();
                /* LR ou HR : le seul vocabulaire que voit l'utilisateur. Des
                   tuiles que le moteur écarte ne font pas une carte HR, si
                   lourdes soient-elles : la ligne dirait le contraire de ce que
                   montre le vol. */
                if (!tuilesEfficaces(c)) {
                    ImGui::TextDisabled("LR");
                } else if (c.tuilesInachevees) {
                    ImGui::TextDisabled("HR (partiel)");
                } else if (c.tuiles) {
                    ImGui::TextUnformatted("HR");
                } else {
                    ImGui::TextDisabled("HR (éteintes)");
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(formaterOctets(c.octetsSocle).c_str());

                ImGui::TableNextColumn();
                /* Une croix, et non le tiret des cartes simplement dépourvues de
                   tuiles : celle-ci n'en aura jamais, son orthophoto étant déjà à
                   la finesse de la source. Le tiret laisserait croire qu'il suffit
                   de les télécharger. */
                if (c.octetsTuiles == 0 && !c.interet.vaut) {
                    ImGui::TextUnformatted("x");
                } else {
                    ImGui::TextUnformatted(formaterOctets(c.octetsTuiles).c_str());
                }

                ImGui::TableNextColumn();
                /* Un état, comme les arbres, et non un poids : l'écran ne sait pas
                   supprimer un buildings.bin, les éteindre ne rend donc pas un
                   octet. Afficher leurs mégaoctets laissait croire le contraire.
                   Ils sont comptés dans le socle, qui est bien ce qu'on récupère
                   en retirant la carte. */
                if (c.octetsBatiments == 0) {
                    ImGui::TextDisabled("aucun");
                } else if (c.batiments) {
                    ImGui::TextUnformatted("oui");
                } else {
                    ImGui::TextDisabled("non");
                }

                ImGui::TableNextColumn();
                /* Les arbres n'occupent aucun disque : leur colonne ne montre
                   donc pas une taille mais un état, et le prix est en images par
                   seconde. */
                if (c.arbres) {
                    ImGui::TextUnformatted("oui");
                } else {
                    ImGui::TextDisabled("non");
                }
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        /* État de la fabrication lu UNE fois par image, avant tout affichage qui
           en dépend : la zone de détail comme le panneau de suivi doivent parler
           du même instant. */
        const fab::Avancement av = fabrique.avancement();
        const EtatCarte&      c  = cartes[selection];

        /* Hauteur figée, pour la même raison que la largeur l'est plus haut : tout
           ce qui suit change de nombre de lignes d'une carte à l'autre et d'un
           état à l'autre, et la fenêtre, centrée à l'écran, se déplaçait donc
           verticalement à chaque flèche. On réserve une fois pour toutes la place
           du cas le plus haut, l'annonce avant fabrication. Ce qui déborderait
           malgré tout reste atteignable : la zone défile. */
        const float reserve = 8.0f * ImGui::GetTextLineHeightWithSpacing() +
                              2.0f * ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("zone", ImVec2(0.0f, reserve));

        ImGui::TextWrapped("%s -- %s", c.dir.c_str(), c.titre.c_str());
        /* Pendant une fabrication et tant que son compte rendu est affiché,
           l'inventaire date d'avant : taire ces lignes plutôt qu'annoncer
           "pas de tuiles" juste au-dessus d'un "terminé, 2371 tuiles". Elles se
           taisent aussi devant l'annonce de fabrication : à ce moment-là l'écran
           doit dire ce que l'action va coûter, pas ce que la carte contient. */
        if (!fabrique.enCours() && !av.termine && aFabriquer != static_cast<int>(selection)) {
            /* Ce qui n'a pas été réglé pour cette carte suit la configuration
               générale : le dire, sinon rien ne distingue un choix pris ici d'une
               valeur héritée, et on ne saurait pas ce qu'un changement général
               viendrait encore modifier. */
            std::string herites;
            if (!c.arbresDefini) {
                herites += "arbres";
            }
            if (!c.batimentsDefini) {
                herites += herites.empty() ? "bâtiments" : ", bâtiments";
            }
            if (!c.tuilesDefinie && c.octetsTuiles > 0) {
                herites += herites.empty() ? "tuiles" : ", tuiles";
            }
            if (!herites.empty()) {
                ImGui::TextDisabled("Suit la configuration générale : %s", herites.c_str());
            }
            if (c.octetsTuiles > 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                ImGui::TextWrapped("Tuiles : %s", c.dossierTuiles.string().c_str());
                ImGui::PopStyleColor();
            } else if (c.interet.vaut) {
                ImGui::TextDisabled("Pas de tuiles de détail : le sol reste flou au ras du sol.");
            } else {
                ImGui::TextDisabled("Pas de tuiles de détail : l'orthophoto d'ensemble suffit "
                                    "ici.");
            }

            /* Le seul chiffre qui décide de la netteté du sol est le RAPPORT
               entre la finesse des tuiles et celle de l'orthophoto d'ensemble.
               Sans lui, on peut télécharger un gigaoctet de tuiles qui ne
               changent rien à l'image, ce qu'aucune taille en mégaoctets ne
               laisse deviner. Le moteur écarte d'ailleurs au chargement un jeu
               qui n'est pas plus fin (voir render::Terrain::ouvrirDetail). */
            /* Fabrication interrompue : c'est la première chose à dire, avant
               toute comparaison de finesse. Les tuiles présentes ne couvrent
               qu'une part de la carte, et la relance reprendra où elle en est. */
            if (c.tuilesInachevees) {
                ImGui::TextWrapped("Fabrication interrompue : %d tuiles écrites sur %d. "
                                   "Relancer reprendra où elle s'est arrêtée.",
                                   c.tuilesPresentes, c.tuilesAttendues);
            }
            if (c.interet.ortho > 0.0f && c.finesseTuiles > 0.0f) {
                const float gain = c.interet.ortho / c.finesseTuiles;
                if (tuilesEfficaces(c)) {
                    ImGui::TextDisabled("Orthophoto %.2f m/px, tuiles %.2f m/px : %.1f fois plus "
                                        "net au ras du sol.",
                                        static_cast<double>(c.interet.ortho),
                                        static_cast<double>(c.finesseTuiles),
                                        static_cast<double>(gain));
                } else {
                    /* En clair, et non en gris : cette ligne explique pourquoi le
                       bouton de fabrication reste éteint. Grisée parmi les autres
                       lignes grises, elle passerait inaperçue et l'écran donnerait
                       l'impression de ne rien faire. */
                    ImGui::TextWrapped("Orthophoto %.2f m/px, tuiles %.2f m/px : pas plus fines, "
                                       "le moteur les ignore. À refaire ou à supprimer.",
                                       static_cast<double>(c.interet.ortho),
                                       static_cast<double>(c.finesseTuiles));
                }
            } else if (c.interet.ortho > 0.0f && !c.interet.vaut) {
                ImGui::TextWrapped("Rien à fabriquer ici : l'orthophoto est déjà à %.2f m/px, la "
                                   "finesse de la source. Des tuiles ne rendraient pas le sol "
                                   "plus net.",
                                   static_cast<double>(c.interet.ortho));
            } else if (c.interet.ortho > 0.0f) {
                ImGui::TextDisabled("Orthophoto %.2f m/px ; des tuiles à %.2f m/px la rendraient "
                                    "%.1f fois plus nette.",
                                    static_cast<double>(c.interet.ortho),
                                    static_cast<double>(c.interet.visee),
                                    static_cast<double>(c.interet.ortho / c.interet.visee));
            }
        }

        /* Fabrication en cours : elle prend toute la place en bas de l'écran, et
           rien d'autre ne doit pouvoir être lancé pendant ce temps. */
        /* L'état est relu à chaque image, y compris APRÈS la fin : le compte rendu
           n'est écrit qu'au tout dernier moment, quand la fabrication ne tourne
           déjà plus. Le garder dans une copie prise pendant le travail le faisait
           manquer, et l'écran restait alors sur un inventaire périmé. */

        if (fabrique.enCours() || av.termine) {
            ImGui::Text("%s", av.message.c_str());
            if (av.blocsTotal > 0) {
                const float part = static_cast<float>(av.blocsFaits) /
                                   static_cast<float>(av.blocsTotal);
                char etiquette[128];
                std::snprintf(etiquette, sizeof(etiquette), "%d / %d blocs, %d tuiles, %s",
                              av.blocsFaits, av.blocsTotal, av.tuilesEcrites,
                              formaterOctets(av.octetsEcrits).c_str());
                /* La barre prend par défaut la couleur des histogrammes, un jaune
                   vif sur lequel le texte blanc d'ImGui devient illisible. On
                   reprend le bleu des boutons, déjà éprouvé sous ce même texte. */
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.26f, 0.45f, 0.68f, 1.0f));
                ImGui::ProgressBar(part, ImVec2(ui::hud_widgets::sc(420.0f), 0.0f), etiquette);
                ImGui::PopStyleColor();
            }
            /* Débit et durée restante seulement pendant le travail, et seulement
               une fois mesurés : tant qu'aucun bloc n'est revenu, on ne sait rien
               et on ne prétend rien. Une fois terminé, ces deux chiffres n'ont
               plus d'objet, le compte rendu prend leur place. Le tourniquet, lui,
               tourne d'un bout à l'autre : c'est le seul élément de l'écran qui
               bouge entre deux blocs, et donc le seul qui dise que ça avance. */
            if (fabrique.enCours()) {
                const char rouet = caractereTournant(glfwGetTime());
                if (av.octetsParSeconde > 0.0) {
                    ImGui::TextDisabled("%c Débit IGN : %s, encore %s", rouet,
                                        formaterDebit(av.octetsParSeconde).c_str(),
                                        formaterDuree(av.secondesRestantes).c_str());
                } else {
                    ImGui::TextDisabled("%c Débit IGN : pas encore mesuré, premier bloc en cours.",
                                        rouet);
                }
            }
            if (fabrique.enCours()) {
                if (ImGui::Button("Arrêter (Échap)", ImVec2(ui::hud_widgets::sc(160.0f), 0.0f))) {
                    fabrique.annuler();
                }
            } else if (ImGui::Button("Fermer (Entrée)",
                                     ImVec2(ui::hud_widgets::sc(160.0f), 0.0f))) {
                /* Ce qui vient d'être fabriqué change les tailles : on réinventorie
                   plutôt que d'afficher des chiffres périmés. */
                fabrique.oublier();
                cartes    = inventorierCartes(assets);
                selection = std::min(selection, cartes.size() - 1);
            }
        } else if (aFabriquer == static_cast<int>(selection)) {
            /* Annonce AVANT d'agir : place occupée, volume à télécharger, durée
               probable. Personne ne doit découvrir après coup qu'il vient de
               lancer deux gigaoctets. La phrase passe à la ligne : la largeur de
               la fenêtre est figée, et ce qui la dépasse serait coupé sans que
               rien ne le signale. */
            ImGui::TextWrapped("%s", estimation.detail.c_str());
            /* Sur une reprise, ces chiffres décrivent le jeu ENTIER : dire ce qui
               est déjà là évite de croire qu'on va tout retélécharger. */
            if (c.tuilesInachevees) {
                ImGui::TextWrapped("Reprise : %d tuiles déjà écrites (%s), seules les manquantes "
                                   "seront téléchargées.",
                                   c.tuilesPresentes, formaterOctets(c.octetsTuiles).c_str());
            } else if (c.finesseTuiles > 0.0f &&
                       std::fabs(c.finesseTuiles - finesseAFabriquer(c)) > 1e-4f) {
                /* Rien de destructeur sans annonce : deux finesses ne se
                   mélangent pas dans un même dossier, l'ancien jeu part. */
                ImGui::TextWrapped("Les tuiles en place (%.2f m/px, %s) seront effacées : deux "
                                   "finesses ne se mélangent pas.",
                                   static_cast<double>(c.finesseTuiles),
                                   formaterOctets(c.octetsTuiles).c_str());
            }

            /* Où cela va-t-il atterrir, et que restera-t-il sur CE disque ? Sans
               ces deux lignes, on peut remplir son disque système sans l'avoir
               voulu, en croyant écrire sur le disque des tuiles. */
            const std::filesystem::path cible = destinationTuiles(cartes[selection]);
            ImGui::TextWrapped("Destination : %s", cible.string().c_str());
            std::error_code ecPlace;
            /* Le dossier n'existe pas encore : on interroge son parent le plus
               proche qui existe, sinon space() échouerait. */
            std::filesystem::path sonde = cible;
            while (!sonde.empty() && !std::filesystem::exists(sonde)) {
                sonde = sonde.parent_path();
            }
            const auto placeCible = std::filesystem::space(sonde, ecPlace);
            if (!ecPlace) {
                const std::uintmax_t apres =
                    (placeCible.available > estimation.octetsDisque)
                        ? placeCible.available - estimation.octetsDisque
                        : 0;
                ImGui::Text("Ce disque : %s libres, %s après",
                            formaterOctets(placeCible.available).c_str(),
                            formaterOctets(apres).c_str());
            }
            const bool tientSurLeDisque =
                ecPlace ||
                placeCible.available > estimation.octetsDisque + 500ull * 1000ull * 1000ull;
            if (!tientSurLeDisque) {
                ImGui::TextUnformatted("Place insuffisante sur le disque de destination.");
            }
            if (tientSurLeDisque && estimation.valide &&
                ImGui::Button("Lancer (Entrée)", ImVec2(ui::hud_widgets::sc(160.0f), 0.0f))) {
                lancerFabrication(cartes[selection]);
                aFabriquer = -1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Renoncer", ImVec2(ui::hud_widgets::sc(120.0f), 0.0f))) {
                aFabriquer = -1;
            }
        } else if (aSupprimer == static_cast<int>(selection)) {
            ImGui::TextUnformatted("Supprimer les tuiles de cette carte ?");
            ImGui::SameLine();
            if (ImGui::Button("Confirmer (Entrée)", ImVec2(ui::hud_widgets::sc(160.0f), 0.0f))) {
                supprimerTuiles(cartes[selection]);
                aSupprimer = -1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Annuler", ImVec2(ui::hud_widgets::sc(120.0f), 0.0f))) {
                aSupprimer = -1;
            }
        } else {
            /* Deux rangées, et les MÊMES boutons quelle que soit la carte : ceux
               qui ne s'appliquent pas sont grisés plutôt que retirés. La fenêtre
               s'ajuste à son contenu, si bien que faire apparaître des boutons
               sur une carte tuilée la faisait bondir en largeur d'un tiers à
               chaque déplacement dans la liste. Les libellés ne rappellent plus
               leur touche : la ligne d'aide, juste en dessous, les donne toutes.

               Première rangée : ce qui se règle carte par carte. */
            EtatCarte& choisie = cartes[selection];
            if (ImGui::Button(choisie.arbres ? "Arbres : oui" : "Arbres : non",
                              ImVec2(ui::hud_widgets::sc(150.0f), 0.0f))) {
                choisie.arbres       = !choisie.arbres;
                choisie.arbresDefini = true;
                ecrireOptions(choisie);
            }
            ImGui::SameLine();
            if (ImGui::Button(choisie.batiments ? "Bâtiments : oui" : "Bâtiments : non",
                              ImVec2(ui::hud_widgets::sc(170.0f), 0.0f))) {
                choisie.batiments       = !choisie.batiments;
                choisie.batimentsDefini = true;
                ecrireOptions(choisie);
            }
            ImGui::SameLine();
            /* Sans tuiles sur le disque, le bouton ne dit pas "oui" : ce réglage
               n'allume rien, et l'afficher armé laisserait croire que la carte est
               tuilée alors qu'il n'y a rien à allumer. */
            ImGui::BeginDisabled(choisie.octetsTuiles == 0);
            const char* etiquetteTuiles = (choisie.octetsTuiles == 0) ? "Tuiles : aucune"
                                          : choisie.tuiles            ? "Tuiles : oui"
                                                                      : "Tuiles : non";
            if (ImGui::Button(etiquetteTuiles, ImVec2(ui::hud_widgets::sc(150.0f), 0.0f))) {
                choisie.tuiles        = !choisie.tuiles;
                choisie.tuilesDefinie = true;
                ecrireOptions(choisie);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!choisie.arbresDefini && !choisie.batimentsDefini &&
                                 !choisie.tuilesDefinie);
            if (ImGui::Button("Réglages par défaut", ImVec2(ui::hud_widgets::sc(200.0f), 0.0f))) {
                rendreAuDefaut(choisie);
            }
            ImGui::EndDisabled();

            /* Seconde rangée : ce qui touche au disque, et la sortie. */
            /* Un jeu entamé se reprend, même sur une carte qui n'aurait rien à
               gagner à en recevoir un neuf : le laisser à moitié écrit serait le
               pire des états. */
            ImGui::BeginDisabled(!fab::reseauDisponible() ||
                                 (!choisie.interet.vaut && !choisie.tuilesInachevees));
            if (ImGui::Button(choisie.tuilesInachevees ? "Reprendre la fabrication"
                                                       : "Fabriquer les tuiles",
                              ImVec2(ui::hud_widgets::sc(200.0f), 0.0f))) {
                estimation = fab::estimer(choisie.dossier, finesseAFabriquer(choisie));
                aFabriquer = static_cast<int>(selection);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(choisie.octetsTuiles == 0);
            if (ImGui::Button("Supprimer les tuiles",
                              ImVec2(ui::hud_widgets::sc(200.0f), 0.0f))) {
                aSupprimer = static_cast<int>(selection);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Retour", ImVec2(ui::hud_widgets::sc(150.0f), 0.0f))) {
                fini = true;
            }
            if (!fab::reseauDisponible()) {
                ImGui::TextDisabled("Compilé sans libcurl : la fabrication est indisponible.");
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextDisabled("Flèches : choisir   Entrée : fabriquer les tuiles   "
                            "Suppr : les supprimer");
        ImGui::TextDisabled("A : arbres   B : bâtiments   T : allumer ou éteindre les tuiles   "
                            "R : réglages par défaut   Échap : retour");
        ImGui::TextDisabled("Les arbres n'occupent aucun disque : ils coûtent des images par "
                            "seconde, pas des mégaoctets.");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        /* Capture de diagnostic de cet écran, comme ARTOUSTE_SCREENSHOT le fait
           pour le vol : indispensable pour vérifier une couleur ou un libellé
           sans avoir à piloter le menu à la main. Lit le tampon arrière avant
           l'échange, quand il porte encore l'image dessinée. */
        if (const char* shot = std::getenv("ARTOUSTE_SHOT_CARTES");
            shot != nullptr && shot[0] != '\0' && ++images >= 3) {
            glFinish();
            std::vector<unsigned char> px(static_cast<std::size_t>(fbw) *
                                          static_cast<std::size_t>(fbh) * 3u);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, fbw, fbh, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            stbi_flip_vertically_on_write(1);
            stbi_write_png(shot, fbw, fbh, 3, px.data(), fbw * 3);
            std::printf("[cartes] capture %s\n", shot);
            fini = true;
        }

        glfwSwapBuffers(m_window);
    }
}

}  /* namespace artouste::app */
