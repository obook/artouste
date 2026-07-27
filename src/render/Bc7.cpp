/*
 * Bc7.cpp
 * Implémentation de la compression BC7 par bandes (voir Bc7.hpp).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#include "render/Bc7.hpp"

#include <bc7enc.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace artouste::render::bc7 {

namespace {

constexpr int         COTE_BLOC     = 4;   /* un bloc BC7 couvre 4x4 pixels */
constexpr std::size_t OCTETS_BLOC   = 16;
constexpr int         LIGNES_MINI   = 8;   /* bande plancher, voir tailleBande */

/* bc7enc exige une initialisation unique avant tout appel, sous peine
   d'artefacts. On la fait à la première compression plutôt qu'au démarrage :
   un joueur qui n'ouvre jamais une carte sans cache n'en a pas besoin. */
const bc7enc_compress_block_params& parametres() {
    static const bc7enc_compress_block_params p = [] {
        bc7enc_compress_block_init();
        bc7enc_compress_block_params params{};
        bc7enc_compress_block_params_init(&params);
        /* Pondération perceptuelle par défaut : elle privilégie la luminance,
           à laquelle l'oeil est le plus sensible. Sur une orthophoto, dont le
           contenu est une photographie et non des couleurs plates, c'est le
           bon choix. m_uber_level reste à 0, son défaut : les niveaux
           supérieurs coûtent beaucoup de temps pour un gain que la mesure ne
           montre pas sur ce contenu. */
        return params;
    }();
    return p;
}

/* Décalage en octets du pixel (x, y) dans une image RGBA de cette largeur. Tout
   passe en size_t d'un coup : ne convertir que le premier terme, comme on le
   faisait, laissait la largeur et l'abscisse en int, que la multiplication
   promeut ensuite en silence. Le compilateur le signalait à juste titre. */
[[nodiscard]] constexpr std::size_t decalagePixel(int x, int y, int largeur) noexcept {
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(largeur) +
            static_cast<std::size_t>(x)) *
           4u;
}

/* Réduit une image RGBA de moitié par moyenne de 2x2 pixels. Les dimensions
   impaires sont gérées en bornant les indices : le dernier pixel d'une ligne
   ou d'une colonne impaire se moyenne avec lui-même plutôt que de déborder. */
std::vector<unsigned char> reduireMoitie(const unsigned char* source, int largeur, int hauteur,
                                         int& largeurReduite, int& hauteurReduite) {
    largeurReduite = std::max(1, largeur / 2);
    hauteurReduite = std::max(1, hauteur / 2);

    std::vector<unsigned char> reduite(static_cast<std::size_t>(largeurReduite) *
                                       static_cast<std::size_t>(hauteurReduite) * 4);
    for (int y = 0; y < hauteurReduite; ++y) {
        const int y0 = std::min(2 * y, hauteur - 1);
        const int y1 = std::min(2 * y + 1, hauteur - 1);
        for (int x = 0; x < largeurReduite; ++x) {
            const int x0 = std::min(2 * x, largeur - 1);
            const int x1 = std::min(2 * x + 1, largeur - 1);
            for (std::size_t c = 0; c < 4; ++c) {
                /* Somme en int, et non en unsigned : quatre octets font au plus
                   1020, et la promotion entière des octets donne déjà un int. Le
                   demander non signé imposait une conversion de plus. */
                const int somme = source[decalagePixel(x0, y0, largeur) + c] +
                                  source[decalagePixel(x1, y0, largeur) + c] +
                                  source[decalagePixel(x0, y1, largeur) + c] +
                                  source[decalagePixel(x1, y1, largeur) + c];
                reduite[decalagePixel(x, y, largeurReduite) + c] =
                    static_cast<unsigned char>(somme / 4);
            }
        }
    }
    return reduite;
}

/* Extrait les 4x4 pixels d'un bloc. Les blocs de bordure débordent quand la
   largeur ou la hauteur n'est pas multiple de 4 : on répète alors le dernier
   pixel valide, ce qui évite d'introduire du noir que le filtrage ferait
   ensuite baver sur le bord visible de la texture. */
void extraireBloc(const unsigned char* image, int largeur, int hauteur, int blocX, int blocY,
                  unsigned char* bloc) {
    for (int j = 0; j < COTE_BLOC; ++j) {
        const int y = std::min(blocY * COTE_BLOC + j, hauteur - 1);
        for (int i = 0; i < COTE_BLOC; ++i) {
            const int x = std::min(blocX * COTE_BLOC + i, largeur - 1);
            const std::size_t src = decalagePixel(x, y, largeur);
            const std::size_t dst = decalagePixel(i, j, COTE_BLOC);
            bloc[dst + 0] = image[src + 0];
            bloc[dst + 1] = image[src + 1];
            bloc[dst + 2] = image[src + 2];
            bloc[dst + 3] = image[src + 3];
        }
    }
}

/* Compresse les lignes de blocs [premiere, derniere) d'un niveau. Appelée
   depuis plusieurs fils, chacun sur une plage disjointe : aucune écriture
   partagée, donc aucune synchronisation nécessaire. */
void compresserLignes(const unsigned char* image, int largeur, int hauteur, int blocsX,
                      int premiere, int derniere, unsigned char* sortie) {
    const auto&   params = parametres();
    unsigned char bloc[COTE_BLOC * COTE_BLOC * 4];
    for (int by = premiere; by < derniere; ++by) {
        for (int bx = 0; bx < blocsX; ++bx) {
            extraireBloc(image, largeur, hauteur, bx, by, bloc);
            unsigned char* destination =
                sortie + (static_cast<std::size_t>(by) * static_cast<std::size_t>(blocsX) +
                          static_cast<std::size_t>(bx)) *
                             OCTETS_BLOC;
            bc7enc_compress_block(destination, bloc, &params);
        }
    }
}

/* Hauteur d'une bande, en lignes de blocs. Compromis entre la finesse de la
   progression et le coût de création des fils : on vise une centaine de
   bandes sur l'ensemble du travail, avec un plancher pour que les petits
   niveaux de mipmap ne soient pas découpés en bandes dérisoires. */
int tailleBande(int blocsYTotal) {
    return std::max(LIGNES_MINI, blocsYTotal / 100);
}

}  /* namespace */

std::optional<dds::Image> compresser(const unsigned char* pixelsRgba, int largeur, int hauteur,
                                     const Progression& progression) {
    if (pixelsRgba == nullptr || largeur <= 0 || hauteur <= 0) {
        return std::nullopt;
    }

    /* Le nombre de niveaux et la taille totale sont connus d'avance : on
       réserve le tampon une fois pour toutes plutôt que de le faire grandir
       niveau par niveau, ce qui recopierait des centaines de mégaoctets. */
    dds::Image image;
    image.largeur = largeur;
    image.hauteur = hauteur;
    {
        int l = largeur, h = hauteur;
        std::size_t total = 0;
        for (;;) {
            const std::size_t octets = dds::octetsBc7(l, h);
            image.niveaux.push_back(dds::Niveau{l, h, total, octets});
            total += octets;
            if (l == 1 && h == 1) {
                break;
            }
            l = std::max(1, l / 2);
            h = std::max(1, h / 2);
        }
        image.donnees.resize(total);
    }

    /* Travail total, en lignes de blocs tous niveaux confondus : sert à
       exprimer la progression sur l'ensemble et non niveau par niveau, sans
       quoi la barre repartirait en arrière à chaque mipmap. */
    std::size_t travailTotal = 0;
    for (const dds::Niveau& n : image.niveaux) {
        travailTotal += static_cast<std::size_t>((n.hauteur + COTE_BLOC - 1) / COTE_BLOC);
    }
    std::size_t travailFait = 0;

    const unsigned nbFils =
        std::max(1u, std::min(std::thread::hardware_concurrency(), 16u));

    /* Niveau 0 : on lit directement les pixels de l'appelant. Les suivants sont
       réduits l'un après l'autre, en ne gardant que le niveau courant : garder
       toute la chaîne en RGBA coûterait un tiers de plus que la source. */
    const unsigned char*       courant = pixelsRgba;
    std::vector<unsigned char> tampon;

    for (std::size_t niveau = 0; niveau < image.niveaux.size(); ++niveau) {
        const dds::Niveau& n      = image.niveaux[niveau];
        const int          blocsX = (n.largeur + COTE_BLOC - 1) / COTE_BLOC;
        const int          blocsY = (n.hauteur + COTE_BLOC - 1) / COTE_BLOC;
        unsigned char*     sortie = image.donnees.data() + n.decalage;

        const int bande = tailleBande(blocsY);
        for (int debut = 0; debut < blocsY; debut += bande) {
            const int fin = std::min(debut + bande, blocsY);

            /* Les lignes de la bande sont réparties entre les fils, puis on
               les attend toutes avant de rendre la main : c'est ce qui garantit
               que le rappel de progression n'est appelé que depuis le fil
               appelant. */
            std::vector<std::thread> fils;
            const int                lignes = fin - debut;
            const int parPaquet = std::max(1, (lignes + static_cast<int>(nbFils) - 1) /
                                                  static_cast<int>(nbFils));
            for (int d = debut; d < fin; d += parPaquet) {
                const int f = std::min(d + parPaquet, fin);
                fils.emplace_back(compresserLignes, courant, n.largeur, n.hauteur, blocsX, d, f,
                                  sortie);
            }
            for (std::thread& fil : fils) {
                fil.join();
            }

            travailFait += static_cast<std::size_t>(lignes);
            if (progression) {
                const float fraction =
                    static_cast<float>(travailFait) / static_cast<float>(travailTotal);
                if (!progression(std::min(1.0f, fraction))) {
                    return std::nullopt;  /* annulé par l'appelant */
                }
            }
        }

        if (niveau + 1 < image.niveaux.size()) {
            int l = 0, h = 0;
            tampon  = reduireMoitie(courant, n.largeur, n.hauteur, l, h);
            courant = tampon.data();
        }
    }

    return image;
}

}  /* namespace artouste::render::bc7 */
