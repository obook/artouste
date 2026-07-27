/*
 * Fenetre.hpp
 * Fenêtre de détail : la portion du jeu de tuiles (voir Pyramide.hpp) tenue en
 * mémoire vidéo autour de l'appareil, et qui suit ses déplacements.
 *
 * Le principe est celui d'un tore. La fenêtre est une seule texture carrée,
 * découpée en emplacements de la taille d'une tuile. Une tuile de la carte va
 * toujours dans l'emplacement de mêmes indices modulo le nombre
 * d'emplacements : quand l'appareil avance, la nouvelle colonne de tuiles
 * écrase donc exactement celle qui vient de sortir par l'arrière, sans rien
 * déplacer et sans jamais réécrire le reste de la texture. La correspondance
 * entre un point du monde et un pixel de la fenêtre est fixe, ce qui évite de
 * transmettre au shader une origine qui bouge.
 *
 * Ce qui coûte cher, ce n'est plus l'emprise de la carte mais la seule fenêtre :
 * la mémoire vidéo occupée est constante, qu'on survole deux kilomètres carrés
 * ou toute la côte landaise.
 *
 * Un shader ne peut pas savoir si l'emplacement qu'il échantillonne contient
 * bien la tuile attendue : une tuile encore en cours de lecture y laisserait des
 * blocs vides, donc du noir. D'où la petite texture de résidence, un pixel par
 * emplacement, que le shader lit d'abord : 0 pour ignorer le détail et garder
 * l'orthophoto d'ensemble, 1 pour l'utiliser pleinement, et l'intervalle pour
 * l'apparition progressive d'une tuile qui vient d'arriver.
 *
 * La lecture des fichiers se fait dans un fil de fond : sur un disque externe
 * ou un support lent, une seule tuile peut demander plusieurs dizaines de
 * millisecondes, soit plusieurs images perdues. Seul l'envoi au GPU reste sur le
 * fil de rendu, où il est obligatoire, et il est plafonné par image.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "render/Dds.hpp"
#include "render/tuiles/Pyramide.hpp"

namespace artouste::render::tuiles {

/* Côté de la fenêtre, en pixels. À 512 pixels par tuile cela fait 16 x 16
   emplacements, soit 89 Mo de mémoire vidéo en BC7 avec les cinq niveaux de
   réduction conservés. Au sol, la fenêtre couvre d'autant plus de terrain que
   les tuiles sont grossières : 2 km de côté à 0,25 m/px, 12 km à 1,5 m/px.
   C'est le seul chiffre à baisser (4096) sur une machine à faible mémoire
   vidéo. */
inline constexpr int COTE_FENETRE_PX = 8192;

/* Niveaux de réduction (mipmaps) conservés dans la fenêtre, du plus fin au plus
   grossier. On s'arrête volontairement tôt : les niveaux suivants ne servent
   qu'au lointain, où l'orthophoto d'ensemble a déjà repris la main, et le
   filtrage y mélangerait des tuiles voisines à travers la couture du tore. */
inline constexpr int NIVEAUX_FENETRE = 5;

/* Durée d'apparition d'une tuile qui vient d'être envoyée au GPU. Assez courte
   pour ne pas se voir, assez longue pour que le passage du flou au net ne soit
   pas un claquement. */
inline constexpr float DUREE_FONDU_S = 0.35f;

/* Tuiles envoyées au GPU par image, au maximum. Une tuile de 512 pixels et ses
   niveaux de réduction pèsent 341 Ko : quatre par image restent indolores, et
   la fenêtre se remplit en une seconde. */
inline constexpr int TUILES_PAR_IMAGE = 4;

class Fenetre {
public:
    /* Prépare la fenêtre pour un jeu de tuiles donné. Le côté doit être un
       multiple du côté de tuile ; il est ajusté à la baisse sinon. Demande un
       contexte OpenGL courant. Si le pilote ne gère pas BC7, ou si la mémoire
       vidéo est refusée, la fenêtre reste inactive : le terrain s'affiche alors
       comme avant, avec sa seule orthophoto d'ensemble. */
    Fenetre(Pyramide pyramide, int cotePx = COTE_FENETRE_PX);
    ~Fenetre();

    /* Ni copiable ni déplaçable : possède des ressources OpenGL et un fil. */
    Fenetre(const Fenetre&)            = delete;
    Fenetre& operator=(const Fenetre&) = delete;
    Fenetre(Fenetre&&)                 = delete;
    Fenetre& operator=(Fenetre&&)      = delete;

    [[nodiscard]] bool active() const noexcept { return m_texture != 0; }

    /* Recentre la fenêtre sur le point monde (x, z), demande les tuiles
       manquantes, envoie au GPU celles qui sont prêtes et fait avancer les
       fondus de dt secondes. À appeler une fois par image, avant de dessiner le
       terrain. */
    void suivre(float x, float z, float dt);

    /* Attache la fenêtre et sa texture de résidence aux unités demandées. */
    void bind(unsigned int uniteFenetre, unsigned int uniteResidence) const;

    /* Coin nord-ouest de la grille de tuiles, en coordonnées monde : c'est
       l'ancre fixe du tore, que le shader utilise pour situer un fragment. */
    [[nodiscard]] float ancreX() const noexcept { return m_pyramide.calage().coinX; }
    [[nodiscard]] float ancreZ() const noexcept { return m_pyramide.calage().coinZ; }

    /* Côté de la fenêtre au sol, en mètres : la période du tore. */
    [[nodiscard]] float tailleM() const noexcept {
        return static_cast<float>(m_cotePx) * m_pyramide.calage().mParPixel;
    }

    [[nodiscard]] float mParPixel() const noexcept { return m_pyramide.calage().mParPixel; }

    /* Distances de fondu du détail vers l'orthophoto d'ensemble : plein détail
       en deçà de la première, plus rien au-delà de la seconde. La seconde reste
       en retrait du bord de la fenêtre, dont les tuiles ne sont pas garanties
       présentes. */
    [[nodiscard]] float rayonPleinM() const noexcept;
    [[nodiscard]] float rayonFonduM() const noexcept;

    /* Nombre d'emplacements portant la bonne tuile, entièrement apparue. Sert à
       la trace et au diagnostic. */
    [[nodiscard]] int residentes() const noexcept;

    /* Nombre de tuiles que la fenêtre devrait porter à sa position actuelle :
       les emplacements dont la tuile existe dans la carte. Toujours inférieur au
       nombre d'emplacements près d'un bord de carte. Comparé à residentes(), il
       dit si le remplissage est terminé : la capture d'écran s'en sert pour
       attendre une image nette (voir ApplicationCapture.cpp). */
    [[nodiscard]] int attendues() const noexcept { return m_attendues; }

    /* Vrai quand il n'y a plus rien à attendre : chaque emplacement sous la
       fenêtre porte sa tuile entièrement apparue, ou bien sa tuile est
       introuvable (hors couverture de la photo aérienne, fichier manquant) et ne
       viendra jamais. Sans ce second cas, une capture d'écran d'une carte de
       montagne, dont les tuiles au-delà de la frontière n'existent pas,
       attendrait en vain. */
    [[nodiscard]] bool stabilisee() const noexcept;

private:
    /* Une tuile demandée au fil de fond. */
    struct Demande {
        int col    = 0;
        int rangee = 0;
    };

    /* Une tuile lue, en attente d'envoi au GPU. */
    struct Prete {
        int       col    = 0;
        int       rangee = 0;
        dds::Image image;
    };

    /* État d'un emplacement de la fenêtre. */
    struct Emplacement {
        int   col    = -1;  /* tuile présente dans la texture, -1 si aucune */
        int   rangee = -1;
        float fondu  = 0.0f;
        /* Tuile cherchée en vain (fichier absent ou abîmé) : on ne la redemande
           pas tant que la fenêtre reste sur ce carré. */
        int absCol    = -1;
        int absRangee = -1;
    };

    void        allouerTexture();
    void        allouerResidence();
    void        envoyer(const Prete& prete);
    void        majResidence();
    void        boucleLecture();
    [[nodiscard]] Emplacement&       emplacement(int col, int rangee) noexcept;
    [[nodiscard]] const Emplacement& emplacement(int col, int rangee) const noexcept;

    Pyramide m_pyramide;
    int      m_cotePx    = 0;
    int      m_nbTuiles  = 0;  /* emplacements par côté */

    unsigned int m_texture   = 0;
    unsigned int m_residence = 0;

    std::vector<Emplacement>   m_emplacements;  /* m_nbTuiles x m_nbTuiles */
    std::vector<unsigned char> m_masque;        /* résidence, un octet par emplacement */
    int                        m_attendues = 0; /* tuiles existantes sous la fenêtre */

    /* Fil de lecture et ses deux files. Le fil dort tant qu'il n'y a rien à
       lire, et s'arrête sur m_fini. */
    std::thread             m_fil;
    std::mutex              m_mutex;
    std::condition_variable m_signal;
    std::deque<Demande>     m_demandes;
    std::deque<Prete>       m_pretes;
    /* Tuile en cours de lecture par le fil de fond, sous m_mutex : le fil de
       rendu s'en sert pour ne pas la redemander. */
    Demande                 m_enLecture{-1, -1};
    std::atomic<bool>       m_fini{false};
};

}  /* namespace artouste::render::tuiles */
