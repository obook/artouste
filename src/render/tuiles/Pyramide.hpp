/*
 * Pyramide.hpp
 * Jeu de tuiles de détail livré à côté d'une carte : l'orthophoto fine découpée
 * en carrés de taille fixe, chacun compressé en BC7 dans son propre fichier DDS
 * (voir Dds.hpp).
 *
 * Pourquoi découper. Une orthophoto d'un seul tenant tient toute en mémoire
 * vidéo, et c'est ce budget qui plafonne la finesse au sol : sur une emprise de
 * 35 x 49 km, descendre à 1,5 m/px demanderait 780 mégapixels, soit 780 Mo même
 * compressés. Découpée, la même donnée se charge par morceaux autour de
 * l'appareil : la mémoire vidéo occupée ne dépend plus de l'emprise de la carte
 * mais de la seule fenêtre entretenue (voir Fenetre.hpp), et l'emprise peut
 * grandir sans rien coûter de plus.
 *
 * Le repère de la grille est le repère MONDE du projet (X vers l'est, Z vers le
 * sud), ancré sur le coin nord-ouest de la tuile (0, 0) : la rangée 0 est au
 * nord, comme la rangée 0 d'une heightmap ou d'une orthophoto IGN. Rien n'est
 * relatif au centre de la carte, si bien qu'une carte recadrée (origin_x /
 * origin_z de terrain.txt) n'a pas de cas particulier.
 *
 * Une carte peut en fournir PLUSIEURS, de finesses différentes : un niveau
 * moyen sur toute l'emprise, et un niveau fin réservé aux endroits où l'on
 * descend vraiment (aires de poser, fond de vallée). C'est ce qui permet d'être
 * net au ras du sol sans payer la finesse maximale sur des centaines de
 * kilomètres carrés : à 0,20 m/px, couvrir la vallée d'Ossau entière
 * demanderait dix gigaoctets, contre quelques centaines de mégaoctets pour les
 * abords d'un lac.
 *
 * Chaque niveau est une grille indépendante, décrite par son propre index :
 *
 *   <carte>/index.txt          niveau unique, ou niveau le plus large
 *   <carte>/<nom>/index.txt    niveau supplémentaire, un sous-dossier par niveau
 *
 * Les niveaux sont reconnus à leur index, pas à leur nom, et classés par
 * finesse : un jeu existant à un seul niveau reste donc lisible tel quel.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace artouste::render::tuiles {

/* Nom du fichier d'index, à la racine du dossier de tuiles. */
inline constexpr const char* NOM_INDEX = "index.txt";

/* Calage de la grille de tuiles en coordonnées monde. */
struct Calage {
    int   tuilePx   = 512;    /* côté d'une tuile, en pixels (multiple de 4 : blocs BC7) */
    float mParPixel = 0.0f;   /* finesse au sol, en mètres par pixel */
    int   colonnes  = 0;      /* nombre de tuiles d'ouest en est */
    int   rangees   = 0;      /* nombre de tuiles du nord au sud */
    float coinX     = 0.0f;   /* coin nord-ouest de la tuile (0, 0), en monde (m) */
    float coinZ     = 0.0f;

    /* Côté d'une tuile au sol, en mètres. */
    [[nodiscard]] float tuileM() const noexcept {
        return static_cast<float>(tuilePx) * mParPixel;
    }

    /* Un calage inutilisable (finesse ou grille nulle) ne doit jamais servir :
       la fenêtre de détail se désactive alors et l'orthophoto d'ensemble reste
       seule, comme avant ce mécanisme. */
    [[nodiscard]] bool valide() const noexcept {
        return tuilePx >= 4 && tuilePx % 4 == 0 && mParPixel > 0.0f && colonnes > 0 &&
               rangees > 0;
    }
};

class Pyramide {
public:
    Pyramide(std::filesystem::path dossier, const Calage& calage)
        : m_dossier(std::move(dossier)), m_calage(calage) {}

    /* Ouvre le jeu de tuiles décrit par <dossier>/index.txt. Renvoie un
       optionnel vide si l'index manque, est illisible ou décrit une grille
       incohérente : l'appelant continue alors sans détail. */
    [[nodiscard]] static std::optional<Pyramide> ouvrir(const std::filesystem::path& dossier);

    /* Écrit <dossier>/index.txt (crée les dossiers manquants). Renvoie faux sur
       échec d'écriture. Utilisé par l'outil de découpage. */
    [[nodiscard]] bool ecrireIndex() const;

    [[nodiscard]] const Calage&                  calage() const noexcept { return m_calage; }
    [[nodiscard]] const std::filesystem::path&   dossier() const noexcept { return m_dossier; }

    /* Emprise couverte par la grille, en mètres. */
    [[nodiscard]] float largeurM() const noexcept {
        return static_cast<float>(m_calage.colonnes) * m_calage.tuileM();
    }
    [[nodiscard]] float hauteurM() const noexcept {
        return static_cast<float>(m_calage.rangees) * m_calage.tuileM();
    }

    /* Tuile contenant le point monde (x, z). Renvoie faux, sans toucher col ni
       rangee, si le point tombe hors de l'emprise couverte. */
    [[nodiscard]] bool tuileEn(float x, float z, int& col, int& rangee) const noexcept;

    /* Vrai si (col, rangee) désigne une tuile de la grille. */
    [[nodiscard]] bool dansGrille(int col, int rangee) const noexcept {
        return col >= 0 && rangee >= 0 && col < m_calage.colonnes && rangee < m_calage.rangees;
    }

    /* Coin nord-ouest d'une tuile, en coordonnées monde. Défini même hors
       grille : la fenêtre de détail s'en sert pour situer ses bords. */
    void coinTuile(int col, int rangee, float& x, float& z) const noexcept;

    /* Chemin du fichier d'une tuile, qu'il existe ou non. Une tuile par
       rangée-dossier : une carte fine en compte des dizaines de milliers, et un
       seul dossier à plat devient pénible à lister comme à copier. */
    [[nodiscard]] std::filesystem::path fichier(int col, int rangee) const;

private:
    std::filesystem::path m_dossier;
    Calage                m_calage;
};

/* Tous les niveaux d'un jeu de tuiles : l'index à la racine s'il existe, plus
   celui de chaque sous-dossier qui en a un. Classés du plus large au plus fin
   (mètres par pixel décroissants). Vide si le dossier n'est pas un jeu de
   tuiles. */
[[nodiscard]] std::vector<Pyramide> ouvrirNiveaux(const std::filesystem::path& dossier);

/* Où sont rangées les tuiles d'une carte, ou un chemin vide si elle n'en a pas.

   On regarde d'abord sous la RACINE demandée (clé "tuiles_dossier" de la
   configuration, ou variable d'environnement ARTOUSTE_TUILES qui prime sur
   elle) : les tuiles d'une carte fine pèsent des gigaoctets, et on les garde
   volontiers sur un autre disque que le jeu. On retombe ensuite sur le
   sous-dossier "tuiles" de la carte, emplacement d'un paquet installé
   normalement.

   Sous une racine, les deux rangements sont acceptés : le jeu de tuiles
   directement dans <racine>/<carte>, ou dans <racine>/<carte>/tuiles. La racine
   par défaut, "assets/terrain", tombe ainsi sur le second sans rien avoir à
   régler.

   Le moteur et le gestionnaire de cartes doivent chercher au même endroit, d'où
   cette fonction partagée. */
[[nodiscard]] std::filesystem::path cheminJeuDeTuiles(const std::filesystem::path& dossierCarte,
                                                      const std::filesystem::path& racine = {});

}  /* namespace artouste::render::tuiles */
