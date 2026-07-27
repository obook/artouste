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
 * endroit. Ils ne se ressemblent pourtant qu'en apparence, et l'écran le dit :
 * les bâtiments sont de la DONNÉE dont on récupère la place, les arbres ne sont
 * qu'un coût d'images par seconde. Le choix est écrit dans le options.txt de la
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
[[nodiscard]] std::uintmax_t tailleDossier(const std::filesystem::path& dossier) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dossier, ec)) {
        return 0;
    }
    std::uintmax_t total = 0;
    for (const auto& entree :
         std::filesystem::recursive_directory_iterator(dossier, ec)) {
        if (entree.is_regular_file(ec)) {
            total += entree.file_size(ec);
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

/* Écrit le options.txt d'une carte. Les deux clés y sont toujours, pour que le
   fichier se relise et s'édite à la main sans surprise. */
bool ecrireOptionsCarte(const std::filesystem::path& dossier, bool arbres, bool batiments,
                        bool tuiles) {
    std::ofstream out(dossier / "options.txt", std::ios::trunc);
    if (!out) {
        return false;
    }
    out << "# Options de la carte, écrites par le gestionnaire de cartes.\n";
    out << "# Le moteur les relit au chargement ; une clé retirée rend la main à\n";
    out << "# la configuration générale (assets/config.txt).\n";
    out << "arbres " << (arbres ? 1 : 0) << "\n";
    out << "batiments " << (batiments ? 1 : 0) << "\n";
    out << "tuiles " << (tuiles ? 1 : 0) << "\n";
    return out.good();
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
        etat.dossierTuiles    = render::tuiles::cheminJeuDeTuiles(dossier);
        etat.octetsTuiles     = tailleDossier(etat.dossierTuiles);
        /* Le socle, c'est tout ce que porte le dossier de la carte, moins les
           bâtiments comptés à part et moins les tuiles si elles y sont rangées. */
        const std::uintmax_t brut = tailleDossier(dossier);
        const std::uintmax_t tuilesDedans =
            (etat.dossierTuiles.empty() || etat.dossierTuiles.parent_path() != dossier)
                ? 0
                : etat.octetsTuiles;
        etat.octetsSocle = brut - std::min(brut, etat.octetsBatiments + tuilesDedans);

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
                etat.arbres = oui;
            } else if (cle == "batiments") {
                etat.batiments = oui;
            } else if (cle == "tuiles") {
                etat.tuiles = oui;
            }
        }
        etats.push_back(std::move(etat));
    }
    return etats;
}

void Application::runGestionnaireCartes() {
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
       que la suivre. finesse est la finesse visée ; 0,75 m/px est le compromis
       retenu pour une carte entière (voir docs/CARTES.md). */
    fab::Fabrique   fabrique;
    fab::Estimation estimation;
    float           finesse = 0.75f;

    /* Les deux actions lourdes sont écrites une seule fois : le clavier et les
       boutons de souris passent par elles, sans quoi les deux chemins finiraient
       par diverger. */
    /* Destination des tuiles : celle qui existe déjà, sinon celle où le moteur
       ira les chercher (ARTOUSTE_TUILES, ou la carte elle-même). Calculée par une
       seule fonction, car l'annonce doit nommer EXACTEMENT le dossier que le
       lancement va remplir. */
    const auto destinationTuiles = [](const EtatCarte& c) {
        if (!c.dossierTuiles.empty()) {
            return c.dossierTuiles;
        }
        if (const char* racine = std::getenv("ARTOUSTE_TUILES");
            racine != nullptr && racine[0] != '\0') {
            return std::filesystem::path(racine) / c.dir;
        }
        return c.dossier / "tuiles";
    };
    const auto lancerFabrication = [&fabrique, &finesse, &destinationTuiles](EtatCarte& c) {
        fabrique.lancer(c.dossier, destinationTuiles(c), finesse);
    };
    const auto supprimerTuiles = [](EtatCarte& c) {
        std::error_code effacement;
        std::filesystem::remove_all(c.dossierTuiles, effacement);
        c.octetsTuiles = 0;
        c.dossierTuiles.clear();
    };

    bool pvHaut = false, pvBas = false, pvRetour = false, pvArbres = false, pvBatiments = false,
         pvTuiles = false, pvValider = false, pvSupprimer = false;
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

        const bool frontRetour    = edge(retour, pvRetour);
        const bool frontValider   = edge(valider, pvValider);
        const bool frontSupprimer = edge(supprimer, pvSupprimer);
        const bool frontArbres    = edge(bascArbres, pvArbres);
        const bool frontBatiments = edge(bascBatiments, pvBatiments);
        const bool frontTuiles    = edge(bascTuiles, pvTuiles);
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
               l'annonce, jamais le téléchargement directement. */
            if (frontValider && fab::reseauDisponible()) {
                estimation = fab::estimer(courante.dossier, finesse);
                aFabriquer = static_cast<int>(selection);
            }
            if (frontSupprimer && courante.octetsTuiles > 0) {
                aSupprimer = static_cast<int>(selection);
            }
            if (frontArbres) {
                courante.arbres = !courante.arbres;
                ecrireOptionsCarte(courante.dossier, courante.arbres, courante.batiments,
                                   courante.tuiles);
            }
            if (frontBatiments) {
                courante.batiments = !courante.batiments;
                ecrireOptionsCarte(courante.dossier, courante.arbres, courante.batiments,
                                   courante.tuiles);
            }
            /* Éteindre les tuiles n'a de sens que si la carte en a : sinon la
               touche écrirait une option sans effet, et l'écran afficherait un
               état que rien ne justifie. */
            if (frontTuiles && courante.octetsTuiles > 0) {
                courante.tuiles = !courante.tuiles;
                ecrireOptionsCarte(courante.dossier, courante.arbres, courante.batiments,
                                   courante.tuiles);
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
        ImGui::Begin("Artouste -- cartes",
                     nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings);

        std::uintmax_t total = 0;
        for (const EtatCarte& c : cartes) {
            total += c.octetsSocle + c.octetsBatiments + c.octetsTuiles;
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
            ImGui::TableSetupColumn("État");
            ImGui::TableSetupColumn("Socle");
            ImGui::TableSetupColumn("Bâtiments");
            ImGui::TableSetupColumn("Tuiles");
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
                /* LR ou HR : le seul vocabulaire que voit l'utilisateur. */
                if (c.octetsTuiles > 0 && c.tuiles) {
                    ImGui::TextUnformatted("HR");
                } else if (c.octetsTuiles > 0) {
                    ImGui::TextDisabled("HR (éteintes)");
                } else {
                    ImGui::TextDisabled("LR");
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(formaterOctets(c.octetsSocle).c_str());

                ImGui::TableNextColumn();
                if (c.octetsBatiments == 0) {
                    ImGui::TextDisabled("aucun");
                } else if (c.batiments) {
                    ImGui::TextUnformatted(formaterOctets(c.octetsBatiments).c_str());
                } else {
                    ImGui::TextDisabled("%s (éteints)",
                                        formaterOctets(c.octetsBatiments).c_str());
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(formaterOctets(c.octetsTuiles).c_str());

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
        ImGui::Text("%s -- %s", c.dir.c_str(), c.titre.c_str());
        /* Pendant une fabrication et tant que son compte rendu est affiché,
           l'inventaire date d'avant : taire cette ligne plutôt qu'annoncer
           "pas de tuiles" juste au-dessus d'un "terminé, 2371 tuiles". */
        if (!fabrique.enCours() && !av.termine) {
            if (c.octetsTuiles > 0) {
                ImGui::TextDisabled("Tuiles : %s", c.dossierTuiles.string().c_str());
            } else {
                ImGui::TextDisabled("Pas de tuiles de détail : le sol reste flou au ras du sol.");
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
            /* Débit et durée restante seulement une fois mesurés : tant qu'aucun
               bloc n'est revenu, on ne sait rien et on ne prétend rien. */
            if (av.octetsParSeconde > 0.0) {
                ImGui::TextDisabled("%.1f Mo/s mesurés, environ %.0f minutes restantes",
                                    av.octetsParSeconde / 1e6, av.secondesRestantes / 60.0);
            } else if (!av.termine) {
                ImGui::TextDisabled("Débit pas encore mesuré : premier bloc en cours.");
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
               lancer deux gigaoctets. */
            ImGui::TextUnformatted(estimation.detail.c_str());

            /* Où cela va-t-il atterrir, et que restera-t-il sur CE disque ? Sans
               ces deux lignes, on peut remplir son disque système sans l'avoir
               voulu, en croyant écrire sur le disque des tuiles. */
            const std::filesystem::path cible = destinationTuiles(cartes[selection]);
            ImGui::Text("Destination : %s", cible.string().c_str());
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
            if (ImGui::Button(cartes[selection].arbres ? "Arbres : oui" : "Arbres : non",
                              ImVec2(ui::hud_widgets::sc(150.0f), 0.0f))) {
                cartes[selection].arbres = !cartes[selection].arbres;
                ecrireOptionsCarte(cartes[selection].dossier, cartes[selection].arbres,
                                   cartes[selection].batiments, cartes[selection].tuiles);
            }
            ImGui::SameLine();
            if (ImGui::Button(cartes[selection].batiments ? "Bâtiments : oui" : "Bâtiments : non",
                              ImVec2(ui::hud_widgets::sc(170.0f), 0.0f))) {
                cartes[selection].batiments = !cartes[selection].batiments;
                ecrireOptionsCarte(cartes[selection].dossier, cartes[selection].arbres,
                                   cartes[selection].batiments, cartes[selection].tuiles);
            }
            if (cartes[selection].octetsTuiles > 0) {
                ImGui::SameLine();
                if (ImGui::Button(cartes[selection].tuiles ? "Tuiles : oui" : "Tuiles : non",
                                  ImVec2(ui::hud_widgets::sc(150.0f), 0.0f))) {
                    cartes[selection].tuiles = !cartes[selection].tuiles;
                    ecrireOptionsCarte(cartes[selection].dossier, cartes[selection].arbres,
                                       cartes[selection].batiments, cartes[selection].tuiles);
                }
                ImGui::SameLine();
                if (ImGui::Button("Supprimer les tuiles (Suppr)",
                                  ImVec2(ui::hud_widgets::sc(260.0f), 0.0f))) {
                    aSupprimer = static_cast<int>(selection);
                }
            }
            /* Fabriquer : proposé quand la carte n'a pas encore ses tuiles, ou
               pour compléter un jeu interrompu. Sans libcurl, on le dit plutôt
               que d'afficher un bouton sans effet. */
            ImGui::SameLine();
            if (!fab::reseauDisponible()) {
                ImGui::TextDisabled("(compilé sans réseau)");
            } else if (ImGui::Button("Fabriquer les tuiles (Entrée)",
                                     ImVec2(ui::hud_widgets::sc(260.0f), 0.0f))) {
                estimation = fab::estimer(cartes[selection].dossier, finesse);
                aFabriquer = static_cast<int>(selection);
            }
            ImGui::SameLine();
            if (ImGui::Button("Retour", ImVec2(ui::hud_widgets::sc(120.0f), 0.0f))) {
                fini = true;
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Flèches : choisir   Entrée : fabriquer les tuiles   "
                            "Suppr : les supprimer");
        ImGui::TextDisabled("A : arbres   B : bâtiments   T : allumer ou éteindre les tuiles   "
                            "Échap : retour");
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
