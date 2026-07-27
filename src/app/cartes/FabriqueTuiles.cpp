/*
 * FabriqueTuiles.cpp
 * Implémentation de la fabrication des tuiles (voir FabriqueTuiles.hpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "app/cartes/FabriqueTuiles.hpp"

#include "render/Bc7.hpp"
#include "render/Dds.hpp"
#include "render/tuiles/Pyramide.hpp"

#include <stb_image.h>

#ifdef ARTOUSTE_HAS_CURL
#include <curl/curl.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

namespace artouste::app::cartes {

namespace {

/* Service d'orthophotos de la Géoplateforme IGN (BD ORTHO), le même que celui
   des scripts de préparation des cartes (tools/terrain/config.py). Données sous
   Licence Ouverte Etalab 2.0. */
constexpr const char* WMS_URL   = "https://data.geopf.fr/wms-r/wms";
constexpr const char* WMS_LAYER = "ORTHOIMAGERY.ORTHOPHOTOS";

/* Une tuile de 512 px pèse environ 350 Ko en BC7 ; le bloc JPEG qui la produit
   est bien plus léger. Sert à annoncer un ordre de grandeur du téléchargement
   avant d'avoir mesuré quoi que ce soit. */
constexpr double OCTETS_JPEG_PAR_PIXEL = 0.25;

/* Cadence de fabrication, tuiles par seconde, découpage et compression BC7
   compris. Mesurée en cours de route sur une fabrication réelle : 4,3 à 4,5
   tuiles par seconde, deux relevés concordants. On retient 4,0, un peu en
   dessous, pour ne pas promettre plus court que nature.

   Ne pas la déduire de l'horodatage d'un jeu terminé : l'écart entre la première
   et la dernière tuile inclut les pauses et les interruptions, ce qui donnait
   1,8 et faisait annoncer le double du temps réel. Cette valeur dépend de la
   machine et n'est qu'un ordre de grandeur ; la mesure du fil de fabrication
   prend le relais dès le premier bloc terminé. */
constexpr double TUILES_PAR_SECONDE = 4.0;

/* Calage d'une carte, lu dans son terrain.txt. */
struct CalageCarte {
    float largeurM = 0.0f;
    float hauteurM = 0.0f;
    float originX  = 0.0f;
    float originZ  = 0.0f;
    float lonMin = 0.0f, lonMax = 0.0f, latMin = 0.0f, latMax = 0.0f;
    /* Hauteur en pixels de l'orthophoto d'ensemble. C'est la seule des deux
       dimensions qui compte : la finesse au sol est l'étendue nord-sud divisée
       par elle, comme la mesure Terrain::orthoMetersPerPixel. Zéro si le fichier
       ne la donne pas, ce qui n'invalide pas le calage. */
    int   orthoHauteur = 0;
    bool  valide = false;
};

[[nodiscard]] CalageCarte lireCalage(const std::filesystem::path& dossierCarte) {
    CalageCarte c;
    std::ifstream in(dossierCarte / "terrain.txt");
    if (!in) {
        return c;
    }
    bool aLargeur = false, aHauteur = false, aGeo0 = false, aGeo1 = false, aGeo2 = false,
         aGeo3 = false;
    std::string cle;
    while (in >> cle) {
        if (!cle.empty() && cle[0] == '#') {
            std::getline(in, cle);
            continue;
        }
        if (cle == "width_m") {
            in >> c.largeurM, aLargeur = true;
        } else if (cle == "height_m") {
            in >> c.hauteurM, aHauteur = true;
        } else if (cle == "origin_x") {
            in >> c.originX;
        } else if (cle == "origin_z") {
            in >> c.originZ;
        } else if (cle == "lon_min") {
            in >> c.lonMin, aGeo0 = true;
        } else if (cle == "lon_max") {
            in >> c.lonMax, aGeo1 = true;
        } else if (cle == "lat_min") {
            in >> c.latMin, aGeo2 = true;
        } else if (cle == "lat_max") {
            in >> c.latMax, aGeo3 = true;
        } else if (cle == "ortho_height") {
            in >> c.orthoHauteur;
        } else {
            std::getline(in, cle);
        }
    }
    c.valide = aLargeur && aHauteur && aGeo0 && aGeo1 && aGeo2 && aGeo3 && c.largeurM > 0.0f &&
               c.hauteurM > 0.0f;
    return c;
}

/* Grille de tuiles couvrant l'emprise. MÊME calcul que l'outil de découpage
   (src/tools/orthotuiles.cpp) et que le script Python : les trois doivent tomber
   d'accord, sans quoi les tuiles ne se rangeraient pas aux mêmes indices. */
[[nodiscard]] render::tuiles::Calage grille(const CalageCarte& carte, float mParPixel) {
    render::tuiles::Calage g;
    g.tuilePx   = TUILE_PX;
    g.mParPixel = mParPixel;
    const float tuileM = static_cast<float>(TUILE_PX) * mParPixel;
    g.colonnes  = static_cast<int>(std::ceil(carte.largeurM / tuileM));
    g.rangees   = static_cast<int>(std::ceil(carte.hauteurM / tuileM));
    g.coinX     = carte.originX - 0.5f * carte.largeurM;
    g.coinZ     = carte.originZ - 0.5f * carte.hauteurM;
    return g;
}

[[nodiscard]] std::string formaterOctets(std::uintmax_t octets) {
    char tampon[32];
    if (octets >= 1000ull * 1000ull * 1000ull) {
        std::snprintf(tampon, sizeof(tampon), "%.1f Go", static_cast<double>(octets) / 1e9);
    } else {
        std::snprintf(tampon, sizeof(tampon), "%.0f Mo", static_cast<double>(octets) / 1e6);
    }
    return tampon;
}

#ifdef ARTOUSTE_HAS_CURL

std::size_t ecrireDansTampon(char* donnees, std::size_t taille, std::size_t nb, void* utilisateur) {
    const std::size_t total = taille * nb;
    auto*             sortie = static_cast<std::vector<unsigned char>*>(utilisateur);
    sortie->insert(sortie->end(), donnees, donnees + total);
    return total;
}

/* Une requête WMS GetMap sur une emprise, en JPEG. Renvoie faux sur échec ; le
   réseau étant capricieux et le service parfois saturé, l'appelant réessaie. */
[[nodiscard]] bool demanderBloc(CURL*                       curl,
                                double                      latLo,
                                double                      lonLo,
                                double                      latHi,
                                double                      lonHi,
                                int                         largeur,
                                int                         hauteur,
                                std::vector<unsigned char>& sortie) {
    char bbox[128];
    std::snprintf(bbox, sizeof(bbox), "%.8f,%.8f,%.8f,%.8f", latLo, lonLo, latHi, lonHi);

    char url[512];
    std::snprintf(url, sizeof(url),
                  "%s?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&LAYERS=%s&STYLES="
                  "&CRS=EPSG:4326&BBOX=%s&WIDTH=%d&HEIGHT=%d&FORMAT=image/jpeg",
                  WMS_URL, WMS_LAYER, bbox, largeur, hauteur);

    sortie.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &ecrireDansTampon);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sortie);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    if (curl_easy_perform(curl) != CURLE_OK) {
        return false;
    }
    /* Le service répond en XML quand il refuse : un JPEG commence par FF D8. */
    return sortie.size() > 2 && sortie[0] == 0xFF && sortie[1] == 0xD8;
}

/* Part de pixels sans donnée (blanc pur) dans une tuile : au-delà, on n'écrit
   pas la tuile et le moteur garde l'orthophoto d'ensemble, recousue à la
   préparation de la carte. Même règle que l'outil de découpage. Sans réseau, il
   n'y a pas de tuile à peser : la fonction reste avec celles qu'elle sert. */
[[nodiscard]] float partBlanche(const std::vector<unsigned char>& tuile) {
    std::size_t       blancs = 0;
    const std::size_t pixels = tuile.size() / 4;
    for (std::size_t i = 0; i < pixels; ++i) {
        if (tuile[i * 4] >= 248 && tuile[i * 4 + 1] >= 248 && tuile[i * 4 + 2] >= 248) {
            ++blancs;
        }
    }
    return (pixels == 0) ? 0.0f : static_cast<float>(blancs) / static_cast<float>(pixels);
}

#endif /* ARTOUSTE_HAS_CURL */

}  /* namespace */

bool reseauDisponible() {
#ifdef ARTOUSTE_HAS_CURL
    return true;
#else
    return false;
#endif
}

bool fabricationInachevee(const std::filesystem::path& dossierTuiles) {
    if (dossierTuiles.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(dossierTuiles / NOM_MARQUEUR_INACHEVE, ec);
}

Interet interet(const std::filesystem::path& dossierCarte) {
    Interet i;
    const CalageCarte carte = lireCalage(dossierCarte);
    if (!carte.valide || carte.orthoHauteur <= 0) {
        i.visee = FINESSE_LA_PLUS_GROSSIERE;
        i.vaut  = true;
        return i;
    }
    i.ortho = carte.hauteurM / static_cast<float>(carte.orthoHauteur);
    i.visee = std::clamp(i.ortho / GAIN_VISE, FINESSE_LA_PLUS_FINE, FINESSE_LA_PLUS_GROSSIERE);
    /* Une carte dont l'orthophoto est déjà à la finesse de la source ne peut rien
       gagner : c'est le cas des petites cartes découpées dans une image fine. */
    i.vaut = i.ortho >= i.visee * GAIN_MINIMUM;
    return i;
}

Estimation estimer(const std::filesystem::path& dossierCarte, float mParPixel) {
    Estimation est;
    const CalageCarte carte = lireCalage(dossierCarte);
    if (!carte.valide || mParPixel <= 0.0f) {
        est.detail = "Calage de carte illisible.";
        return est;
    }

    const render::tuiles::Calage g = grille(carte, mParPixel);
    est.valide   = g.valide();
    est.colonnes = g.colonnes;
    est.rangees  = g.rangees;
    const double pixels = static_cast<double>(g.colonnes) * static_cast<double>(g.rangees) *
                          static_cast<double>(TUILE_PX) * static_cast<double>(TUILE_PX);
    est.octetsDisque = static_cast<std::uintmax_t>(pixels * OCTETS_PAR_PIXEL);
    est.octetsReseau = static_cast<std::uintmax_t>(pixels * OCTETS_JPEG_PAR_PIXEL);
    est.blocs = ((g.colonnes + TUILES_PAR_BLOC - 1) / TUILES_PAR_BLOC) *
                ((g.rangees + TUILES_PAR_BLOC - 1) / TUILES_PAR_BLOC);

    /* Durée : deux plafonds, et c'est le plus haut qui commande. Celui de la
       ligne est une fourchette, on ne connaît pas son débit avant d'avoir reçu
       quelque chose. Celui de la MACHINE est plus régulier : découper un bloc et
       compresser ses tuiles en BC7 tient une cadence à peu près constante, et sur
       une carte fine il domine largement le téléchargement. L'annoncer évite de
       promettre dix minutes pour un travail de quatre heures. La mesure prend le
       relais dès le premier bloc terminé, elle englobe les deux. */
    const double minutesLigne = static_cast<double>(est.octetsReseau) / (1e6 * 60.0);
    const double minutesMachine =
        static_cast<double>(g.colonnes) * static_cast<double>(g.rangees) /
        (TUILES_PAR_SECONDE * 60.0);
    /* Arrondi AVANT de choisir l'unité : sans cela 59,7 minutes s'afficheraient
       "environ 60 minutes", une durée qui se dit en heures. */
    const double minutes =
        std::round(std::max(std::max(minutesLigne, minutesMachine), 1.0));

    char duree[64];
    if (minutes >= 60.0) {
        std::snprintf(duree, sizeof(duree), "environ %.0f h %02.0f", std::floor(minutes / 60.0),
                      minutes - 60.0 * std::floor(minutes / 60.0));
    } else {
        std::snprintf(duree, sizeof(duree), "environ %.0f minutes", minutes);
    }

    char phrase[448];
    std::snprintf(phrase, sizeof(phrase),
                  "%d x %d tuiles à %.2f m/px : %s sur le disque, environ %s à télécharger, "
                  "en %d blocs. Compter %s, la compression pesant autant que la ligne.",
                  g.colonnes, g.rangees, static_cast<double>(mParPixel),
                  formaterOctets(est.octetsDisque).c_str(),
                  formaterOctets(est.octetsReseau).c_str(), est.blocs, duree);
    est.detail = phrase;
    return est;
}

Fabrique::~Fabrique() {
    annuler();
}

bool Fabrique::lancer(const std::filesystem::path& dossierCarte,
                      const std::filesystem::path& dossierSortie,
                      float                        mParPixel) {
    if (m_enCours.load()) {
        return false;
    }
    if (m_fil.joinable()) {
        m_fil.join();  /* fabrication précédente terminée : on récupère son fil */
    }
    m_arret.store(false);
    m_enCours.store(true);
    {
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_avancement = Avancement{};
        m_avancement.message = "Préparation...";
    }
    m_fil = std::thread(&Fabrique::boucle, this, dossierCarte, dossierSortie, mParPixel);
    return true;
}

void Fabrique::annuler() {
    m_arret.store(true);
    if (m_fil.joinable()) {
        m_fil.join();
    }
    m_enCours.store(false);
}

Avancement Fabrique::avancement() const {
    std::lock_guard<std::mutex> verrou(m_mutex);
    return m_avancement;
}

void Fabrique::oublier() {
    if (m_enCours.load()) {
        return;
    }
    std::lock_guard<std::mutex> verrou(m_mutex);
    m_avancement = Avancement{};
}

void Fabrique::boucle(std::filesystem::path dossierCarte,
                      std::filesystem::path dossierSortie,
                      float                 mParPixel) {
    const auto finir = [this](bool echec, const std::string& message) {
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_avancement.termine = true;
        m_avancement.echec   = echec;
        m_avancement.message = message;
        m_enCours.store(false);
    };

#ifndef ARTOUSTE_HAS_CURL
    (void)dossierCarte;
    (void)dossierSortie;
    (void)mParPixel;
    finir(true, "Compilé sans libcurl : rien à faire ici.");
#else
    const CalageCarte carte = lireCalage(dossierCarte);
    if (!carte.valide) {
        finir(true, "Calage de carte illisible.");
        return;
    }

    const render::tuiles::Calage calage = grille(carte, mParPixel);
    if (!calage.valide()) {
        finir(true, "Grille de tuiles inutilisable.");
        return;
    }

    /* Un jeu déjà là, mais à une AUTRE finesse, ne peut pas être complété : ses
       fichiers portent les mêmes noms et couvrent d'autres emprises, si bien que
       la reprise, qui saute les tuiles présentes, mélangerait deux échelles sur
       la même carte. On l'efface donc avant d'écrire le nouvel index. L'écran
       l'annonce dans sa confirmation, la règle de cet écran étant de ne rien
       détruire sans le dire (voir ApplicationMenuCartes.cpp). */
    if (const auto ancien = render::tuiles::Pyramide::ouvrir(dossierSortie)) {
        if (std::fabs(ancien->calage().mParPixel - mParPixel) > 1e-4f) {
            std::error_code ec;
            std::filesystem::remove_all(dossierSortie, ec);
        }
    }

    /* L'index d'abord : c'est lui qui fixe la grille, et le moteur s'y réfère.
       Écrit avant la première tuile, pour qu'une fabrication interrompue laisse
       un jeu cohérent, seulement incomplet. */
    const render::tuiles::Pyramide pyramide{dossierSortie, calage};
    if (!pyramide.ecrireIndex()) {
        finir(true, "Impossible d'écrire l'index dans " + dossierSortie.string());
        return;
    }

    /* Témoin d'inachèvement, posé avec l'index et retiré au tout dernier moment.
       C'est lui, et non l'index, qui dit à l'écran des cartes que le jeu est
       partiel : l'index décrit la grille VOULUE, pas celle qui est sur le disque. */
    {
        std::ofstream marqueur(dossierSortie / NOM_MARQUEUR_INACHEVE, std::ios::trunc);
        marqueur << "# Fabrication en cours ou interrompue.\n";
        marqueur << "# Ce fichier disparaît quand le jeu de tuiles est complet.\n";
        marqueur << "# Relancer la fabrication reprend où elle s'est arrêtée.\n";
        marqueur << "m_par_pixel " << mParPixel << "\n";
        marqueur << "tuiles_attendues " << calage.colonnes * calage.rangees << "\n";
    }

    const int blocsX = (calage.colonnes + TUILES_PAR_BLOC - 1) / TUILES_PAR_BLOC;
    const int blocsY = (calage.rangees + TUILES_PAR_BLOC - 1) / TUILES_PAR_BLOC;
    {
        std::lock_guard<std::mutex> verrou(m_mutex);
        m_avancement.blocsTotal = blocsX * blocsY;
        m_avancement.message    = "Téléchargement...";
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        finir(true, "Initialisation réseau impossible.");
        return;
    }

    const double degParMLon = static_cast<double>(carte.lonMax - carte.lonMin) /
                              static_cast<double>(carte.largeurM);
    const double degParMLat = static_cast<double>(carte.latMax - carte.latMin) /
                              static_cast<double>(carte.hauteurM);
    const double tuileM = static_cast<double>(TUILE_PX) * static_cast<double>(mParPixel);

    const auto debut = std::chrono::steady_clock::now();
    std::vector<unsigned char> jpeg;
    std::vector<unsigned char> tuile(static_cast<std::size_t>(TUILE_PX) *
                                     static_cast<std::size_t>(TUILE_PX) * 4);
    std::uintmax_t octetsRecus = 0, octetsEcrits = 0;
    int            tuilesEcrites = 0;
    bool           erreur = false;

    for (int by = 0; by < blocsY && !m_arret.load() && !erreur; ++by) {
        for (int bx = 0; bx < blocsX && !m_arret.load() && !erreur; ++bx) {
            const int col0    = bx * TUILES_PAR_BLOC;
            const int rangee0 = by * TUILES_PAR_BLOC;
            const int nCol    = std::min(TUILES_PAR_BLOC, calage.colonnes - col0);
            const int nRangee = std::min(TUILES_PAR_BLOC, calage.rangees - rangee0);

            /* Emprise du bloc. La grille est ancrée sur le coin nord-ouest, et la
               rangée 0 est au nord : la latitude décroît quand la rangée croît. */
            const double lonLo = static_cast<double>(carte.lonMin) +
                                 static_cast<double>(col0) * tuileM * degParMLon;
            const double lonHi = lonLo + static_cast<double>(nCol) * tuileM * degParMLon;
            const double latHi = static_cast<double>(carte.latMax) -
                                 static_cast<double>(rangee0) * tuileM * degParMLat;
            const double latLo = latHi - static_cast<double>(nRangee) * tuileM * degParMLat;

            bool recu = false;
            for (int essai = 0; essai < 3 && !recu && !m_arret.load(); ++essai) {
                recu = demanderBloc(curl, latLo, lonLo, latHi, lonHi, nCol * TUILE_PX,
                                    nRangee * TUILE_PX, jpeg);
                if (!recu) {
                    std::this_thread::sleep_for(std::chrono::seconds(2 * (essai + 1)));
                }
            }
            if (m_arret.load()) {
                break;
            }
            if (!recu) {
                erreur = true;
                break;
            }
            octetsRecus += jpeg.size();

            /* Rangée 0 au nord, comme l'écrivent les outils de préparation : on ne
               retourne pas l'image, contrairement au chemin de rendu. */
            stbi_set_flip_vertically_on_load(0);
            int            largeur = 0, hauteur = 0, canaux = 0;
            unsigned char* pixels = stbi_load_from_memory(jpeg.data(),
                                                          static_cast<int>(jpeg.size()), &largeur,
                                                          &hauteur, &canaux, 4);
            if (pixels == nullptr || largeur != nCol * TUILE_PX || hauteur != nRangee * TUILE_PX) {
                if (pixels != nullptr) {
                    stbi_image_free(pixels);
                }
                erreur = true;
                break;
            }

            for (int dr = 0; dr < nRangee && !m_arret.load() && !erreur; ++dr) {
                for (int dc = 0; dc < nCol && !m_arret.load() && !erreur; ++dc) {
                    const std::filesystem::path chemin =
                        pyramide.fichier(col0 + dc, rangee0 + dr);
                    if (std::filesystem::exists(chemin)) {
                        continue;  /* reprise : cette tuile est déjà là */
                    }
                    /* Le bloc est à la finesse cible et aligné sur la grille : une
                       tuile y est une simple recopie, sans rééchantillonnage. */
                    for (int y = 0; y < TUILE_PX; ++y) {
                        const std::size_t source =
                            (static_cast<std::size_t>(dr * TUILE_PX + y) *
                                 static_cast<std::size_t>(largeur) +
                             static_cast<std::size_t>(dc * TUILE_PX)) * 4;
                        std::copy_n(pixels + source, static_cast<std::size_t>(TUILE_PX) * 4,
                                    tuile.begin() + static_cast<std::ptrdiff_t>(y) * TUILE_PX * 4);
                    }
                    if (partBlanche(tuile) > 0.9f) {
                        continue;  /* hors couverture : à l'orthophoto d'ensemble */
                    }
                    const auto blocs =
                        render::bc7::compresser(tuile.data(), TUILE_PX, TUILE_PX, {});
                    if (!blocs.has_value() ||
                        !render::dds::ecrire(chemin, *blocs, render::dds::Empreinte{})) {
                        erreur = true;
                        break;
                    }
                    std::error_code ec;
                    octetsEcrits += std::filesystem::file_size(chemin, ec);
                    ++tuilesEcrites;
                }
            }
            stbi_image_free(pixels);

            const double secondes =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - debut).count();
            const int faits = by * blocsX + bx + 1;
            std::lock_guard<std::mutex> verrou(m_mutex);
            m_avancement.blocsFaits    = faits;
            m_avancement.tuilesEcrites = tuilesEcrites;
            m_avancement.octetsRecus   = octetsRecus;
            m_avancement.octetsEcrits  = octetsEcrits;
            /* Débit et durée restante déduits de ce qui a RÉELLEMENT été reçu,
               jamais d'une vitesse supposée. */
            if (secondes > 0.5 && faits > 0) {
                m_avancement.octetsParSeconde = static_cast<double>(octetsRecus) / secondes;
                m_avancement.secondesRestantes =
                    secondes / static_cast<double>(faits) *
                    static_cast<double>(m_avancement.blocsTotal - faits);
            }
        }
    }

    curl_easy_cleanup(curl);

    if (erreur) {
        finir(true, "Échec réseau ou données inattendues. Ce qui est écrit est conservé : "
                    "relancer reprendra où on en est.");
    } else if (m_arret.load()) {
        finir(false, "Arrêté. Ce qui est écrit est conservé : relancer reprendra où on en est.");
    } else {
        /* Seul chemin qui retire le témoin : la grille a été parcourue en entier,
           sans arrêt ni erreur. */
        std::error_code ec;
        std::filesystem::remove(dossierSortie / NOM_MARQUEUR_INACHEVE, ec);
        finir(false, "Terminé : " + std::to_string(tuilesEcrites) + " tuiles, " +
                         formaterOctets(octetsEcrits) + " sur le disque.");
    }
#endif
}

}  /* namespace artouste::app::cartes */
