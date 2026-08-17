/*
 * FenetreRelief.hpp
 * Fenêtre de relief fin autour de l'appareil, et la grille qui la dessine par-
 * dessus le maillage d'ensemble. Au relief ce que tuiles/Fenetre.hpp est à
 * l'orthophoto : le maillage d'une carte est figé au chargement et plafonné par
 * relief_sommets_max, soit 17,5 m de maille sur 18 km ; la même dépense en
 * sommets autour de l'appareil donne 2 m sur 2 km.
 *
 * Trois choix qui la rendent bien plus courte que la fenêtre d'image :
 *
 * - les altitudes vont dans une TEXTURE et terrain.vert déplace les sommets. La
 *   grille n'est donc jamais reconstruite ni renvoyée au GPU ; ses points se
 *   déduisent de gl_VertexID, seuls ses indices sont stockés ;
 * - la texture est un tore, comme celle de l'image : avancer n'écrase que ce qui
 *   vient de sortir par l'arrière ;
 * - un emplacement dont la tuile manque est rempli avec le relief d'ensemble.
 *   D'où ni texture de résidence, ni sentinelle, ni branche dans le shader.
 *
 * Le raccord se fait par un fondu vers le relief de la carte sur les derniers
 * mètres de la grille, sans découper de trou dans le maillage d'ensemble.
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace artouste::render::relief {

inline constexpr const char* NOM_INDEX = "index.txt";

/* Tuiles par côté du tore. Six pour que l'anneau tienne dedans où que la caméra
   soit dans sa tuile : la marge garantie est de (TUILES_FENETRE / 2 - 1) tuiles,
   soit 2 km. Chaque tuile de plus coûte sa mémoire, en vidéo comme en vive :
   6 x 6 font 37 Mo de chaque côté. */
inline constexpr int TUILES_FENETRE = 6;

/* Les deux grilles dessinées, du noyau vers l'anneau. Le noyau tessellise le
   relief au pas des tuiles, l'anneau le prolonge quatre fois plus grossièrement
   sur quatre fois plus de terrain, pour un quart de ses sommets : 1,05 M sur
   2 km, puis 0,26 M sur 4 km. Les deux lisent les MÊMES altitudes, seule la
   finesse du maillage change, si bien qu'aucune marche ne les sépare.

   Côté du noyau IMPAIR : terrain.vert pose ses sommets à demi = (cote - 1) / 2
   pas du centre, et ce demi ne tombe sur les texels que si le côté est impair.
   Avec 1024, tout le noyau glissait d'un demi-texel et lisait des moyennes de
   quatre texels, ce qui rabote les parois. Ne pas remettre 1024. */
inline constexpr int COTE_GRILLE_POINTS  = 1023;
inline constexpr int COTE_ANNEAU_POINTS  = 512;
inline constexpr int PAS_ANNEAU          = 4;

inline constexpr int TUILES_PAR_IMAGE = 2;

/* Période commune des réseaux en jeu, en mailles : les pas des deux grilles et
   les blocs des niveaux lus. Le centre s'y cale, sans quoi un réseau glisse sur
   l'autre à chaque pas et les normales sautent.

   Pas le pas de la grille la plus grossière : l'anneau passé à 4 m, le centre
   alternerait entre deux phases du réseau de 8 m. */
/* La grille de la fenêtre s'emboîte-t-elle dans la maille de la carte ? Il le
   faut pour le noyau ET pour l'anneau, qui dessine à PAS_ANNEAU fois ce pas :
   sinon la fenêtre redessine la surface du maillage au lieu de la reproduire, et
   les silhouettes se déplacent à sa frontière. */
[[nodiscard]] inline bool emboiteDansMaille(float pasFenetre, float mailleCarte,
                                            int multAnneau) noexcept {
    if (pasFenetre <= 0.0f || mailleCarte <= 0.0f || multAnneau < 1) {
        return false;
    }
    const float k = mailleCarte / (pasFenetre * static_cast<float>(multAnneau));
    return k >= 1.0f && std::fabs(k - std::round(k)) < 1e-3f;
}

inline constexpr int NIVEAU_ANNEAU  = 2;   /* niveau le plus grossier que l'anneau lit */
inline constexpr int CALAGE_MAILLES = 1 << NIVEAU_ANNEAU;

/* Fondu du bord vers le relief d'ensemble. Il rattrape l'écart entre MNT LiDAR
   et RGE ALTI (4 m au sommet du Pic du Midi, 28 m sur une falaise), qui ferait
   sinon une marche.

   RADIAL et large, comme celui de la fenêtre d'image : un fondu carré et court
   dessine un angle droit en travers du paysage, vu en vol le 16/08 sur le
   versant du Pic du Midi. La proportion est celle de tuiles/Fenetre.cpp. */
inline constexpr float BORD_RETRAIT_M = 10.0f;
inline constexpr float BORD_PLEIN     = 0.62f;

/* Maille à laquelle on considère que la carte porte déjà le relief. Le lissage
   de la fenêtre à cette échelle est retranché d'elle-même : ne reste que le
   détail que la carte ne peut pas tenir. Valeur ronde proche de la maille d'une
   carte de montagne (17,5 m sur 18 km). */
inline constexpr float MAILLE_CARTE_M = 16.0f;

/* Calage de la grille de tuiles, en coordonnées monde. Comme tuiles::Calage,
   mais une tuile porte des points d'altitude, sans recouvrement entre tuiles. */
struct Calage {
    int   tuilePoints = 512;
    /* Pas PAR AXE. Il vaut dx/k et dz/k de la carte, k entier, si bien que la
       grille de la fenêtre s'emboîte exactement dans celle du maillage
       d'ensemble. Les deux axes de la carte n'ayant pas la même maille, ces deux
       pas diffèrent : une tuile n'est PAS carrée au sol. */
    float pasX        = 0.0f;
    float pasZ        = 0.0f;
    int   colonnes    = 0;
    int   rangees     = 0;
    float coinX       = 0.0f;
    float coinZ       = 0.0f;

    [[nodiscard]] float tuileX() const noexcept {
        return static_cast<float>(tuilePoints) * pasX;
    }
    [[nodiscard]] float tuileZ() const noexcept {
        return static_cast<float>(tuilePoints) * pasZ;
    }
    [[nodiscard]] bool valide() const noexcept {
        return tuilePoints >= 16 && pasX > 0.0f && pasZ > 0.0f && colonnes > 0 && rangees > 0;
    }
};

class FenetreRelief {
public:
    /* Corrige une tuile avant son entrée dans la fenêtre : coin nord-ouest en
       monde, pas, côté, altitudes à modifier sur place (rangée 0 au nord).

       aDonnee faux : pas de LiDAR ici, tout est à remplir depuis la carte.
       Vrai : reste à ramener vers la carte là où elle a été retouchée, sinon le
       pad de départ aplani ressortirait bosselé.

       Seul endroit où la fenêtre touche à la carte ; la politique reste dans
       Terrain. */
    using Correcteur =
        std::function<void(float x0, float z0, float pasX, float pasZ, int cote, float* hauteurs,
                           bool aDonnee)>;

    /* Ouvre <dossier>/index.txt et alloue la fenêtre. nullptr si l'index manque,
       est illisible, ou si la mémoire vidéo est refusée : l'appelant continue
       alors avec le seul maillage d'ensemble. Demande un contexte OpenGL. */
    [[nodiscard]] static std::unique_ptr<FenetreRelief>
    ouvrir(const std::filesystem::path& dossier, Correcteur correcteur,
           int coteGrillePoints = COTE_GRILLE_POINTS);

    ~FenetreRelief();

    FenetreRelief(const FenetreRelief&)            = delete;
    FenetreRelief& operator=(const FenetreRelief&) = delete;
    FenetreRelief(FenetreRelief&&)                 = delete;
    FenetreRelief& operator=(FenetreRelief&&)      = delete;

    /* Recentre sur (x, z) et prépare au plus TUILES_PAR_IMAGE tuiles. Une fois
       par image, avant le dessin. */
    void suivre(float x, float z);

    void bind(unsigned int unite) const;

    /* Grilles à dessiner, du noyau (0) vers l'anneau. Elles se recouvrent : le
       noyau se dessine en premier et marque le pochoir, l'anneau ne remplit
       ensuite que ce qui reste. Aucune fente ne peut donc s'ouvrir entre deux
       finesses de maillage. */
    [[nodiscard]] int   niveaux() const noexcept { return static_cast<int>(m_grilles.size()); }
    [[nodiscard]] int coteGrille(int niveau) const noexcept {
        return m_grilles[static_cast<std::size_t>(niveau)].cote;
    }
    [[nodiscard]] float pasGrille(int niveau) const noexcept {
        return m_grilles[static_cast<std::size_t>(niveau)].pasX;
    }
    [[nodiscard]] float pasGrilleZ(int niveau) const noexcept {
        return m_grilles[static_cast<std::size_t>(niveau)].pasZ;
    }
    /* Tire une grille ; l'appelant a réglé le shader et le pochoir. */
    void dessiner(int niveau) const;

    /* Détail à AJOUTER au relief d'ensemble sous (x, z), et son poids de fondu.
       Faux hors de la grille, l'appelant gardant le relief d'ensemble seul.

       C'est un écart, pas une altitude : la fenêtre n'apporte que ce que la
       maille de la carte ne sait pas représenter, et lui laisse ses basses
       fréquences. Sans cela, les deux relevés ne s'accordant pas (3,4 m d'écart
       moyen et 11,6 m d'écart-type mesurés sur bigorre), les sommets
       changeraient d'altitude en entrant dans la fenêtre.

       MÊME formule que terrain.vert, sinon l'appareil se poserait ailleurs que
       sur ce qu'il voit. */
    [[nodiscard]] bool detailEn(float x, float z, float hauteurCarte, float& detail,
                                float& poids) const noexcept;

    /* Niveau de réduction qui lisse la fenêtre à la maille de la carte : c'est
       lui qui sépare ce que la carte porte déjà de ce que la fenêtre ajoute. */
    [[nodiscard]] float niveauLissage() const noexcept;

    /* Distance jusqu'à laquelle le détail est lu à sa pleine finesse. Au-delà,
       la résolution lue est divisée par deux à chaque doublement de distance.

       Elle est choisie pour que la finesse lue tombe exactement sur le pas de la
       grille qui la dessine : à la frontière du noyau, l'anneau doit recevoir
       une surface définie à SON pas de 8 m, sinon il l'échantillonne trop
       grossièrement et il en naît des pics qui scintillent. Comme la loi ne
       dépend que de la distance, les deux grilles lisent la même surface là où
       elles se rejoignent. */
    [[nodiscard]] float distanceDetailM() const noexcept;

    /* Ancre du tore : coin nord-ouest de la tuile (0, 0). */
    [[nodiscard]] float ancreX() const noexcept { return m_calage.coinX; }
    [[nodiscard]] float ancreZ() const noexcept { return m_calage.coinZ; }
    /* Période du tore au sol, et son côté en texels. */
    [[nodiscard]] float tailleM() const noexcept {
        return static_cast<float>(m_cotePoints) * m_calage.pasX;
    }
    /* Période du tore par axe : la tuile n'est pas carrée au sol. */
    [[nodiscard]] float tailleZ() const noexcept {
        return static_cast<float>(m_cotePoints) * m_calage.pasZ;
    }
    [[nodiscard]] int   cotePoints() const noexcept { return m_cotePoints; }
    [[nodiscard]] float pasX() const noexcept { return m_calage.pasX; }
    [[nodiscard]] float pasZ() const noexcept { return m_calage.pasZ; }
    [[nodiscard]] float centreX() const noexcept { return m_centreX; }
    [[nodiscard]] float centreZ() const noexcept { return m_centreZ; }
    /* Position continue de l'appareil. Le centre calé pose les sommets, l'oeil
       mesure le fondu et la finesse : calés eux aussi, ils avanceraient par
       bonds de 8 m. */
    [[nodiscard]] float oeilX() const noexcept { return m_oeilX; }
    [[nodiscard]] float oeilZ() const noexcept { return m_oeilZ; }
    /* Demi-côté de la grille la plus large : c'est elle qui porte le fondu. */
    [[nodiscard]] float demiGrilleM() const noexcept {
        const Grille& large = m_grilles.back();
        return 0.5f * static_cast<float>(large.cote - 1) * large.pasX;
    }
    /* Début et fin du fondu, en distance au centre. */
    [[nodiscard]] float fonduFinM() const noexcept { return demiGrilleM() - BORD_RETRAIT_M; }
    [[nodiscard]] float fonduDebutM() const noexcept { return BORD_PLEIN * fonduFinM(); }

private:
    FenetreRelief(std::filesystem::path dossier, const Calage& calage, Correcteur correcteur,
                  int coteGrillePoints);

    /* Une grille dessinée : rien que des indices, les sommets étant calculés par
       terrain.vert depuis gl_VertexID. */
    struct Grille {
        unsigned int vao     = 0;
        unsigned int ebo     = 0;
        int          indices = 0;
        int          cote    = 0;
        float        pasX    = 0.0f;
        float        pasZ    = 0.0f;
    };

    bool allouerTexture();
    bool construireGrille(int cote, float pasX, float pasZ);
    /* Lecture ou remplissage, correction, envoi au GPU et copie CPU. */
    void poserTuile(int col, int rangee);
    [[nodiscard]] bool lireTuile(int col, int rangee, std::vector<float>& hauteurs) const;

    std::filesystem::path m_dossier;
    Calage                m_calage;
    Correcteur            m_correcteur;

    int m_cotePoints = 0; /* côté de la texture torique */

    unsigned int        m_texture = 0;
    std::vector<Grille> m_grilles; /* du noyau vers l'anneau */

    /* Altitudes côté CPU, même disposition torique : c'est ce que lit hauteurEn,
       donc le poser et la collision. */
    std::vector<float> m_hauteurs;

    /* Tuile portée par chaque emplacement du tore, -1 si aucune. */
    struct Emplacement {
        int col    = -1;
        int rangee = -1;
    };
    std::vector<Emplacement> m_emplacements;

    bool  m_premier = true;
    float m_centreX = 0.0f;
    float m_centreZ = 0.0f;
    float m_oeilX   = 0.0f;
    float m_oeilZ   = 0.0f;
};

/* Où sont les tuiles de relief d'une carte, ou un chemin vide. Le jeu est un
   dossier FRÈRE de celui des tuiles d'image, <racine>/<carte>.relief : un
   sous-dossier portant un index.txt serait pris pour un niveau d'image de plus
   (voir tuiles/Pyramide.cpp). */
[[nodiscard]] std::filesystem::path
cheminJeuDeRelief(const std::filesystem::path& dossierCarte, const std::filesystem::path& racine = {});

}  /* namespace artouste::render::relief */
