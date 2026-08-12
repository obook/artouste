/*
 * FabriqueTuiles.hpp
 * Fabrication du jeu de tuiles d'une carte : récupération de l'orthophoto fine
 * auprès de l'IGN, découpage en tuiles et compression BC7. C'est ce qui fait
 * passer une carte de LR à HR depuis le gestionnaire de cartes, sans passer par
 * les scripts Python de l'auteur (voir docs/DISTRIBUTION.md).
 *
 * Le travail se fait dans un fil de fond : il dure des dizaines de minutes,
 * pendant lesquelles la fenêtre doit rester vivante, afficher l'avancement et
 * pouvoir tout arrêter. Rien de ce fil ne touche à OpenGL.
 *
 * Le principe est celui du script qu'il remplace (tools/terrain/fetch_tuiles.py) :
 * on ne matérialise JAMAIS la mosaïque complète, qui ferait des centaines de
 * mégapixels, mais des BLOCS de quelques tuiles, découpés puis jetés aussitôt.
 * La mémoire occupée ne dépend donc pas de la taille de la carte, et une
 * interruption ne coûte que le bloc en cours.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace artouste::app::cartes {

/* Côté d'une tuile, en pixels : celui que lit le moteur (voir tuiles::Calage). */
inline constexpr int TUILE_PX = 512;

/* Tuiles par côté de bloc. Neuf tuiles font 4608 pixels, sous la limite de
   ~5010 px par requête du service IGN. */
inline constexpr int TUILES_PAR_BLOC = 9;

/* Densité du BC7 avec ses niveaux de réduction : 1 octet par pixel, plus un
   tiers pour les niveaux. Sert à annoncer la place occupée AVANT de commencer,
   ce qui est le seul chiffre qu'on puisse promettre exactement. */
inline constexpr double OCTETS_PAR_PIXEL = 4.0 / 3.0;

/* Ce que l'écran affiche pendant le travail. Recopié sous verrou : le fil de
   rendu le lit à chaque image, le fil de fabrication l'écrit. */
struct Avancement {
    int  blocsFaits = 0;
    int  blocsTotal = 0;
    int  tuilesEcrites = 0;
    std::uintmax_t octetsRecus  = 0;  /* venus du réseau */
    std::uintmax_t octetsEcrits = 0;  /* posés sur le disque */
    /* Débit mesuré sur ce qui a réellement été reçu, et durée restante qui s'en
       déduit. Nuls tant qu'aucun bloc n'est terminé : on n'annonce pas un chiffre
       qu'on n'a pas mesuré. */
    double octetsParSeconde = 0.0;
    double secondesRestantes = 0.0;
    bool        termine = false;
    bool        echec   = false;
    std::string message;
};

/* Estimation faite AVANT de lancer quoi que ce soit, à partir du seul calage de
   la carte. La place est exacte ; la durée ne l'est pas et est donc donnée en
   fourchette, jusqu'à ce que la mesure prenne le relais. */
struct Estimation {
    bool           valide = false;
    int            colonnes = 0;
    int            rangees  = 0;
    int            blocs    = 0;
    std::uintmax_t octetsDisque = 0;
    std::uintmax_t octetsReseau = 0;  /* ordre de grandeur du téléchargement */
    std::string    detail;            /* phrase prête à afficher */
};

/* Ce que coûterait la fabrication des tuiles de cette carte à cette finesse.
   Lit son terrain.txt ; renvoie une estimation invalide s'il est illisible ou si
   la finesse demandée n'a pas de sens. */
[[nodiscard]] Estimation estimer(const std::filesystem::path& dossierCarte, float mParPixel);

/* Bornes de la finesse des tuiles, en mètres par pixel. La plus fine est celle de
   la source : la BD ORTHO de l'IGN est à 20 cm, en demander davantage ne ferait
   qu'agrandir les mêmes pixels.

   La plus grossière est celle que reçoivent toutes les grandes cartes, dont le
   tiers d'orthophoto tombe bien au-delà : c'est donc elle, et non le rapport, qui
   décide de ce qu'on voit au ras du sol sur ossau, cote-landes ou arcachon.

   Elle vaut 0,25 m/px, soit 64 % de l'information que porte la source : un pixel
   couvre 1,6 fois la surface du pixel IGN, contre 6 fois à 0,50 et 14 fois à
   0,75, les deux valeurs essayées avant. Le prix est connu et assumé : une carte
   de montagne pèse alors 7 Go et demande une heure et demie de fabrication, à
   quatre tuiles par seconde. */
inline constexpr float FINESSE_LA_PLUS_FINE      = 0.20f;
inline constexpr float FINESSE_LA_PLUS_GROSSIERE = 0.25f;

/* Gain visé sur l'orthophoto d'ensemble, et gain en deçà duquel la fabrication
   ne vaut pas d'être proposée. Trois fois plus fin se voit d'emblée au ras du
   sol ; moitié plus fin ne se distingue pas, et le moteur écarte de toute façon
   un jeu de tuiles qui n'est pas plus fin que l'orthophoto (voir
   render::Terrain::ouvrirDetail). */
inline constexpr float GAIN_VISE    = 3.0f;
inline constexpr float GAIN_MINIMUM = 1.5f;

/* Ce que des tuiles apporteraient à cette carte. La finesse ne peut pas être la
   même partout : une carte de montagne de 18 km porte une orthophoto à 3,6 m/px,
   qu'un jeu de tuiles à 0,75 m/px rend cinq fois plus nette, tandis qu'une petite
   carte d'aérodrome en porte déjà une à 0,85 m/px, sur laquelle les mêmes tuiles
   ne changeraient rien de visible. */
struct Interet {
    float ortho = 0.0f;    /* finesse de l'orthophoto d'ensemble, m/px */
    float visee = 0.0f;    /* finesse à demander aux tuiles, m/px */
    bool  vaut  = false;   /* faux : rien de visible à gagner, ne pas proposer */
};

/* Mesure cet intérêt d'après le terrain.txt de la carte. Un calage muet sur son
   orthophoto laisse la finesse au compromis d'usage, et la fabrication proposée :
   on ne refuse pas une carte qu'on n'a pas su mesurer. */
[[nodiscard]] Interet interet(const std::filesystem::path& dossierCarte);

/* Vrai si le jeu a été compilé avec de quoi aller chercher les données (libcurl).
   Sans elle, l'écran doit le dire au lieu de proposer un bouton sans effet. */
[[nodiscard]] bool reseauDisponible();

/* Marqueur posé dans le dossier de sortie pendant toute la fabrication, et retiré
   à la seule condition qu'elle aille jusqu'au bout. L'index, lui, est écrit dès la
   première tuile : sans ce témoin, un jeu interrompu ne se distingue pas d'un jeu
   complet, et l'écran des cartes annonce des tuiles qui ne couvrent qu'un coin de
   la carte. Il survit à un arrêt demandé comme à une coupure de courant. */
inline constexpr const char* NOM_MARQUEUR_INACHEVE = "fabrication_inachevee.txt";

/* Ce dossier de tuiles porte-t-il la trace d'une fabrication interrompue ? Faux
   pour un dossier vide ou absent. Les scripts de l'auteur posent le même marqueur
   (tools/terrain/fetch_tuiles.py), leurs jeux interrompus sont donc annoncés
   partiels comme les autres ; ceux fabriqués avant qu'ils ne le fassent restent
   tenus pour complets, faute de trace. */
[[nodiscard]] bool fabricationInachevee(const std::filesystem::path& dossierTuiles);

class Fabrique {
public:
    Fabrique() = default;
    ~Fabrique();

    Fabrique(const Fabrique&)            = delete;
    Fabrique& operator=(const Fabrique&) = delete;

    /* Lance la fabrication en fil de fond. dossierSortie reçoit l'index et les
       tuiles. Une fabrication déjà en cours est refusée (renvoie faux). */
    bool lancer(const std::filesystem::path& dossierCarte,
                const std::filesystem::path& dossierSortie,
                float                        mParPixel);

    /* Demande l'arrêt et attend la fin du bloc en cours. Ce qui est écrit reste
       écrit : une reprise ultérieure sautera les tuiles déjà là. */
    void annuler();

    [[nodiscard]] bool       enCours() const noexcept { return m_enCours.load(); }

    /* Dernier état connu. Reste disponible APRÈS la fin : c'est là que se trouve
       le compte rendu, et l'appelant doit pouvoir l'afficher alors que le fil est
       déjà terminé. Sans fabrication lancée, tout est à zéro. */
    [[nodiscard]] Avancement avancement() const;

    /* Oublie le compte rendu, une fois qu'il a été lu et pris en compte. Sans
       effet pendant une fabrication. */
    void oublier();

private:
    void boucle(std::filesystem::path dossierCarte,
                std::filesystem::path dossierSortie,
                float                 mParPixel);

    std::thread       m_fil;
    std::atomic<bool> m_enCours{false};
    std::atomic<bool> m_arret{false};
    mutable std::mutex m_mutex;
    Avancement         m_avancement;
};

}  /* namespace artouste::app::cartes */
