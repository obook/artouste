/*
 * orthotuiles.cpp
 * Découpe l'orthophoto d'une carte en tuiles BC7, le jeu de détail que le
 * simulateur charge par morceaux autour de l'appareil (voir
 * render/tuiles/Pyramide.hpp et render/tuiles/Fenetre.hpp).
 *
 * L'outil est hors du jeu pour deux raisons. Compresser est long : une carte
 * fine représente des dizaines de milliers de tuiles, soit des heures de calcul
 * qu'on ne va pas refaire chez chaque joueur. Et le résultat est la donnée
 * livrée, pas un cache : il se range dans le paquet de la carte, se copie et se
 * vérifie comme un fichier d'assets ordinaire.
 *
 * Usage :
 *   ./build/bin/orthotuiles <dossier-carte> <dossier-sortie> [options]
 *
 *   --m-par-pixel M   finesse cible en mètres par pixel (défaut : celle de la
 *                     source, qu'on ne peut pas dépasser sans inventer du
 *                     détail)
 *   --tuile-px N      côté d'une tuile en pixels, multiple de 4 (défaut 512)
 *   --source F        image à découper (défaut <dossier-carte>/ortho.jpg)
 *   --reprendre       saute les tuiles déjà écrites (reprise après coupure)
 *   --apercu C R F    n'écrit rien : sort la tuile (C, R) en PNG non compressé
 *                     dans F, pour vérifier à l'oeil le cadrage et l'orientation
 *   --index-seul      écrit le seul index.txt, sans découper : sert à préparer
 *                     une grille que des appels --bloc rempliront ensuite
 *   --bloc C R        la source ne couvre pas la carte mais un BLOC de tuiles
 *                     entières dont le coin nord-ouest est la tuile (C, R). Ses
 *                     dimensions doivent être des multiples du côté de tuile.
 *                     C'est le mode utilisé pour une carte fine : l'orthophoto
 *                     complète y pèserait des dizaines de gigaoctets, alors que
 *                     ses blocs se récupèrent et se découpent un par un (voir
 *                     tools/terrain/fetch_tuiles.py).
 *
 * Exemple :
 *   ./build/bin/orthotuiles assets/terrain/capbreton /media/disque/capbreton
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "render/Bc7.hpp"
#include "render/Dds.hpp"
#include "render/tuiles/Pyramide.hpp"

using artouste::render::tuiles::Calage;
using artouste::render::tuiles::Pyramide;
namespace bc7 = artouste::render::bc7;
namespace dds = artouste::render::dds;

namespace {

/* Calage de la carte source, tel que le décrit son terrain.txt. Seules les
   dimensions au sol et le décalage d'origine nous intéressent : la grille de
   tuiles s'ancre sur le coin nord-ouest de l'emprise. */
struct CalageCarte {
    float largeurM = 0.0f;
    float hauteurM = 0.0f;
    float originX  = 0.0f;
    float originZ  = 0.0f;
};

bool lireCalageCarte(const std::filesystem::path& terrainTxt, CalageCarte& out) {
    std::ifstream in(terrainTxt);
    if (!in) {
        return false;
    }
    bool        aLargeur = false, aHauteur = false;
    std::string cle;
    while (in >> cle) {
        if (!cle.empty() && cle[0] == '#') {
            std::getline(in, cle);
            continue;
        }
        if (cle == "width_m") {
            in >> out.largeurM, aLargeur = true;
        } else if (cle == "height_m") {
            in >> out.hauteurM, aHauteur = true;
        } else if (cle == "origin_x") {
            in >> out.originX;
        } else if (cle == "origin_z") {
            in >> out.originZ;
        } else {
            std::getline(in, cle);
        }
    }
    return aLargeur && aHauteur && out.largeurM > 0.0f && out.hauteurM > 0.0f;
}

/* Image RGBA en mémoire, avec sa correspondance au sol. */
struct Source {
    std::vector<unsigned char> pixels;  /* 4 octets par pixel, rangée 0 au nord */
    int                        largeur = 0;
    int                        hauteur = 0;
};

/* Un pixel de la tuile cible couvre en général plusieurs pixels de la source :
   on moyenne son empreinte au lieu d'y prendre un seul échantillon, sinon le
   sous-échantillonnage crénèle les toits et les marquages au sol. Quand
   l'empreinte est plus petite qu'un pixel source (finesse cible proche de la
   source), la moyenne dégénère naturellement en une lecture du pixel couvrant.
   Les bords sont pincés sur l'image : la dernière tuile d'une rangée dépasse
   l'emprise de la carte quand la largeur n'est pas un multiple entier du côté
   de tuile, et prolonge alors la dernière colonne de pixels. */
void moyenne(const Source& src,
             float         x0,
             float         y0,
             float         x1,
             float         y1,
             unsigned char* sortie) {
    const int ix0 = std::clamp(static_cast<int>(std::floor(x0)), 0, src.largeur - 1);
    const int iy0 = std::clamp(static_cast<int>(std::floor(y0)), 0, src.hauteur - 1);
    const int ix1 = std::clamp(static_cast<int>(std::ceil(x1)) - 1, ix0, src.largeur - 1);
    const int iy1 = std::clamp(static_cast<int>(std::ceil(y1)) - 1, iy0, src.hauteur - 1);

    unsigned int somme[3] = {0, 0, 0};
    unsigned int nb       = 0;
    for (int y = iy0; y <= iy1; ++y) {
        const std::size_t ligne = static_cast<std::size_t>(y) * static_cast<std::size_t>(src.largeur);
        for (int x = ix0; x <= ix1; ++x) {
            const std::size_t i = (ligne + static_cast<std::size_t>(x)) * 4;
            somme[0] += src.pixels[i + 0];
            somme[1] += src.pixels[i + 1];
            somme[2] += src.pixels[i + 2];
            ++nb;
        }
    }
    if (nb == 0) {
        nb = 1;
    }
    sortie[0] = static_cast<unsigned char>(somme[0] / nb);
    sortie[1] = static_cast<unsigned char>(somme[1] / nb);
    sortie[2] = static_cast<unsigned char>(somme[2] / nb);
    sortie[3] = 255;
}

/* Remplit une tuile RGBA depuis la source. (col, rangee) situent la tuile dans
   la grille ; l'échelle passe des pixels de tuile aux pixels de source. */
void composerTuile(const Source&  src,
                   const Calage&  calage,
                   float          srcPxParM,
                   int            col,
                   int            rangee,
                   std::vector<unsigned char>& tuile) {
    const float pxParM = srcPxParM;
    const float tuileM = calage.tuileM();
    /* Coin nord-ouest de la tuile, en pixels de la source. La grille est ancrée
       sur le coin nord-ouest de la carte, qui est le pixel (0, 0) de la source. */
    const float baseX = static_cast<float>(col) * tuileM * pxParM;
    const float baseY = static_cast<float>(rangee) * tuileM * pxParM;
    const float pas   = calage.mParPixel * pxParM;  /* pixels source par pixel de tuile */

    for (int y = 0; y < calage.tuilePx; ++y) {
        const float sy0 = baseY + static_cast<float>(y) * pas;
        for (int x = 0; x < calage.tuilePx; ++x) {
            const float   sx0 = baseX + static_cast<float>(x) * pas;
            const std::size_t i =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(calage.tuilePx) +
                 static_cast<std::size_t>(x)) *
                4;
            moyenne(src, sx0, sy0, sx0 + pas, sy0 + pas, tuile.data() + i);
        }
    }
}

/* Mode bloc : la source est déjà à la finesse cible et alignée sur la grille,
   une tuile y occupe donc exactement tuilePx pixels. Simple recopie, sans
   rééchantillonnage : c'est ce qui permet de découper une carte fine que sa
   mosaïque complète rendrait impossible à tenir en mémoire. */
void copierTuile(const Source& src,
                 int           tuilePx,
                 int           colLocale,
                 int           rangeeLocale,
                 std::vector<unsigned char>& tuile) {
    for (int y = 0; y < tuilePx; ++y) {
        const int sy = rangeeLocale * tuilePx + y;
        for (int x = 0; x < tuilePx; ++x) {
            const int         sx = colLocale * tuilePx + x;
            const std::size_t i =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(tuilePx) +
                 static_cast<std::size_t>(x)) * 4;
            const std::size_t j = (static_cast<std::size_t>(sy) *
                                       static_cast<std::size_t>(src.largeur) +
                                   static_cast<std::size_t>(sx)) * 4;
            tuile[i + 0] = src.pixels[j + 0];
            tuile[i + 1] = src.pixels[j + 1];
            tuile[i + 2] = src.pixels[j + 2];
            tuile[i + 3] = 255;
        }
    }
}

/* Part de pixels de "no-data" de la BD ORTHO dans une tuile : le service IGN
   rend du blanc pur là où la couverture s'arrête, typiquement au-delà de la
   frontière espagnole sur une carte de montagne. L'orthophoto d'ensemble, elle,
   a été recousue à la préparation de la carte (voir tools/terrain/ortho.py,
   fill_nodata) : mieux vaut donc ne pas écrire une tuile blanche et laisser le
   moteur retomber sur cette orthophoto, plutôt que de plaquer un carré blanc
   sur le paysage. */
[[nodiscard]] float partBlanche(const std::vector<unsigned char>& tuile) {
    std::size_t blancs = 0;
    const std::size_t pixels = tuile.size() / 4;
    for (std::size_t i = 0; i < pixels; ++i) {
        if (tuile[i * 4 + 0] >= 248 && tuile[i * 4 + 1] >= 248 && tuile[i * 4 + 2] >= 248) {
            ++blancs;
        }
    }
    return (pixels == 0) ? 0.0f : static_cast<float>(blancs) / static_cast<float>(pixels);
}

void usage() {
    std::fprintf(stderr,
                 "Usage : orthotuiles <dossier-carte> <dossier-sortie> [options]\n"
                 "  --m-par-pixel M   finesse cible (défaut : celle de la source)\n"
                 "  --tuile-px N      côté d'une tuile, multiple de 4 (défaut 512)\n"
                 "  --source F        image à découper (défaut <carte>/ortho.jpg)\n"
                 "  --reprendre       saute les tuiles déjà écrites\n"
                 "  --apercu C R F    sort la seule tuile (C, R) en PNG dans F\n"
                 "  --index-seul      écrit le seul index.txt, sans découper\n"
                 "  --bloc C R        la source est un bloc de tuiles à partir de (C, R)\n");
}

}  /* namespace */

int main(int argc, char** argv) {
    if (argc < 3) {
        usage();
        return 1;
    }

    const std::filesystem::path dossierCarte = argv[1];
    const std::filesystem::path dossierSortie = argv[2];
    float                       mParPixelVoulu = 0.0f;
    int                         tuilePx        = 512;
    std::filesystem::path       source         = dossierCarte / "ortho.jpg";
    bool                        reprendre      = false;
    int                         apercuCol      = -1;
    int                         apercuRangee   = -1;
    std::filesystem::path       apercuFichier;
    bool                        indexSeul      = false;
    int                         blocCol        = -1;
    int                         blocRangee     = -1;

    for (int i = 3; i < argc; ++i) {
        const char* a = argv[i];
        const bool  aValeur = (i + 1 < argc);
        if (std::strcmp(a, "--m-par-pixel") == 0 && aValeur) {
            mParPixelVoulu = std::strtof(argv[++i], nullptr);
        } else if (std::strcmp(a, "--tuile-px") == 0 && aValeur) {
            tuilePx = std::atoi(argv[++i]);
        } else if (std::strcmp(a, "--source") == 0 && aValeur) {
            source = argv[++i];
        } else if (std::strcmp(a, "--reprendre") == 0) {
            reprendre = true;
        } else if (std::strcmp(a, "--apercu") == 0 && i + 3 < argc) {
            apercuCol     = std::atoi(argv[++i]);
            apercuRangee  = std::atoi(argv[++i]);
            apercuFichier = argv[++i];
        } else if (std::strcmp(a, "--index-seul") == 0) {
            indexSeul = true;
        } else if (std::strcmp(a, "--bloc") == 0 && i + 2 < argc) {
            blocCol    = std::atoi(argv[++i]);
            blocRangee = std::atoi(argv[++i]);
        } else {
            std::fprintf(stderr, "Option inconnue : %s\n", a);
            usage();
            return 1;
        }
    }

    CalageCarte carte;
    if (!lireCalageCarte(dossierCarte / "terrain.txt", carte)) {
        std::fprintf(stderr,
                     "Calage de carte illisible : %s\n",
                     (dossierCarte / "terrain.txt").string().c_str());
        return 1;
    }

    /* Grille de tuiles d'une carte : ancrée sur son coin nord-ouest, elle couvre
       toute l'emprise, la dernière tuile de chaque rangée et de chaque colonne
       dépassant au besoin. Ne dépend que du calage de la carte et de la finesse
       visée, jamais de l'image fournie : c'est ce qui permet de remplir la même
       grille par blocs successifs. */
    const auto grille = [&carte, tuilePx](float mpp) {
        Calage c;
        c.tuilePx   = tuilePx;
        c.mParPixel = mpp;
        const float tuileM = static_cast<float>(tuilePx) * mpp;
        c.colonnes         = static_cast<int>(std::ceil(carte.largeurM / tuileM));
        c.rangees          = static_cast<int>(std::ceil(carte.hauteurM / tuileM));
        c.coinX     = carte.originX - 0.5f * carte.largeurM;
        c.coinZ     = carte.originZ - 0.5f * carte.hauteurM;
        return c;
    };

    /* Préparation de la grille seule : le pilote de récupération IGN écrit
       l'index une fois, puis remplit les blocs (voir --bloc). Il n'y a alors
       aucune image source à lire, et la finesse doit être donnée. */
    if (indexSeul) {
        if (mParPixelVoulu <= 0.0f) {
            std::fprintf(stderr, "--index-seul demande --m-par-pixel.\n");
            return 1;
        }
        const Calage calage = grille(mParPixelVoulu);
        if (!calage.valide()) {
            std::fprintf(stderr, "Calage de tuiles inutilisable.\n");
            return 1;
        }
        const Pyramide pyramide{dossierSortie, calage};
        if (!pyramide.ecrireIndex()) {
            std::fprintf(stderr, "Impossible d'écrire l'index.\n");
            return 1;
        }
        std::printf("Index écrit : %d x %d tuiles de %d px à %.3f m/px (%.0f m au sol).\n",
                    calage.colonnes,
                    calage.rangees,
                    calage.tuilePx,
                    static_cast<double>(calage.mParPixel),
                    static_cast<double>(calage.tuileM()));
        return 0;
    }

    /* Rangée 0 au nord, comme l'écrit l'outil de préparation des cartes : on ne
       retourne donc PAS l'image au chargement, contrairement au chemin de rendu
       qui, lui, doit s'accorder à l'axe vertical d'OpenGL. */
    stbi_set_flip_vertically_on_load(0);
    Source src;
    int    canaux = 0;
    unsigned char* pixels =
        stbi_load(source.string().c_str(), &src.largeur, &src.hauteur, &canaux, 4);
    if (pixels == nullptr) {
        std::fprintf(stderr, "Image source illisible : %s\n", source.string().c_str());
        return 1;
    }
    src.pixels.assign(pixels,
                      pixels + static_cast<std::size_t>(src.largeur) *
                                   static_cast<std::size_t>(src.hauteur) * 4);
    stbi_image_free(pixels);

    /* Découpage d'un bloc : la grille existe déjà, on ne la recalcule pas et on
       ne touche pas à l'index. Le bloc est à la finesse cible et aligné sur la
       grille, ses tuiles sont donc de simples recopies. */
    if (blocCol >= 0 && blocRangee >= 0) {
        const auto ouverte = Pyramide::ouvrir(dossierSortie);
        if (!ouverte.has_value()) {
            std::fprintf(stderr,
                         "Index absent ou illisible dans %s : lancer d'abord --index-seul.\n",
                         dossierSortie.string().c_str());
            return 1;
        }
        const Calage& calage = ouverte->calage();
        if (src.largeur % calage.tuilePx != 0 || src.hauteur % calage.tuilePx != 0) {
            std::fprintf(stderr,
                         "Bloc de %d x %d px : pas un multiple du côté de tuile (%d).\n",
                         src.largeur,
                         src.hauteur,
                         calage.tuilePx);
            return 1;
        }

        const dds::Empreinte     empreinte = dds::empreinteDe(source);
        std::vector<unsigned char> tuile(static_cast<std::size_t>(calage.tuilePx) *
                                         static_cast<std::size_t>(calage.tuilePx) * 4);
        int ecrites  = 0;
        int blanches = 0;
        for (int dr = 0; dr < src.hauteur / calage.tuilePx; ++dr) {
            for (int dc = 0; dc < src.largeur / calage.tuilePx; ++dc) {
                const int col    = blocCol + dc;
                const int rangee = blocRangee + dr;
                if (!ouverte->dansGrille(col, rangee)) {
                    continue;  /* le bloc dépasse la carte : rien à écrire */
                }
                const std::filesystem::path chemin = ouverte->fichier(col, rangee);
                if (reprendre && std::filesystem::exists(chemin)) {
                    continue;
                }
                copierTuile(src, calage.tuilePx, dc, dr, tuile);
                if (partBlanche(tuile) > 0.9f) {
                    ++blanches;
                    continue;
                }
                const auto blocs =
                    bc7::compresser(tuile.data(), calage.tuilePx, calage.tuilePx, {});
                if (!blocs.has_value() || !dds::ecrire(chemin, *blocs, empreinte)) {
                    std::fprintf(stderr, "Échec sur la tuile (%d, %d).\n", col, rangee);
                    return 1;
                }
                ++ecrites;
            }
        }
        std::printf("Bloc (%d, %d) : %d tuiles écrites", blocCol, blocRangee, ecrites);
        if (blanches > 0) {
            std::printf(", %d hors couverture (laissées à l'orthophoto d'ensemble)", blanches);
        }
        std::printf(".\n");
        return 0;
    }

    /* Finesse de la source : la hauteur au sol divisée par sa hauteur en pixels
       (voir Terrain::orthoMetersPerPixel, même mesure faite sur la texture
       plutôt que lue dans le calage). */
    const float mParPixelSource = carte.hauteurM / static_cast<float>(src.hauteur);
    float       mParPixel       = (mParPixelVoulu > 0.0f) ? mParPixelVoulu : mParPixelSource;
    if (mParPixel < mParPixelSource) {
        std::fprintf(stderr,
                     "Finesse demandée (%.3f m/px) plus fine que la source (%.3f m/px) :\n"
                     "il n'y a pas de détail à y gagner, on reste sur la source.\n",
                     static_cast<double>(mParPixel),
                     static_cast<double>(mParPixelSource));
        mParPixel = mParPixelSource;
    }

    const Calage calage = grille(mParPixel);
    const float  tuileM = calage.tuileM();

    if (!calage.valide()) {
        std::fprintf(stderr,
                     "Calage de tuiles inutilisable (tuile_px %d, m/px %.3f, grille %d x %d).\n",
                     calage.tuilePx,
                     static_cast<double>(calage.mParPixel),
                     calage.colonnes,
                     calage.rangees);
        return 1;
    }

    const float srcPxParMApercu = static_cast<float>(src.hauteur) / carte.hauteurM;
    if (!apercuFichier.empty()) {
        std::vector<unsigned char> apercu(static_cast<std::size_t>(tuilePx) *
                                          static_cast<std::size_t>(tuilePx) * 4);
        composerTuile(src, calage, srcPxParMApercu, apercuCol, apercuRangee, apercu);
        if (stbi_write_png(apercuFichier.string().c_str(),
                           tuilePx,
                           tuilePx,
                           4,
                           apercu.data(),
                           tuilePx * 4) == 0) {
            std::fprintf(stderr, "Écriture de l'aperçu échouée : %s\n",
                         apercuFichier.string().c_str());
            return 1;
        }
        float coinX = 0.0f, coinZ = 0.0f;
        const Pyramide vue{dossierSortie, calage};
        vue.coinTuile(apercuCol, apercuRangee, coinX, coinZ);
        std::printf("Aperçu de la tuile (%d, %d), coin monde (%.1f, %.1f) : %s\n",
                    apercuCol,
                    apercuRangee,
                    static_cast<double>(coinX),
                    static_cast<double>(coinZ),
                    apercuFichier.string().c_str());
        return 0;
    }

    const Pyramide pyramide{dossierSortie, calage};
    if (!pyramide.ecrireIndex()) {
        std::fprintf(stderr,
                     "Impossible d'écrire l'index : %s\n",
                     (dossierSortie / artouste::render::tuiles::NOM_INDEX).string().c_str());
        return 1;
    }

    const int total = calage.colonnes * calage.rangees;
    std::printf("Carte      : %s\n", dossierCarte.filename().string().c_str());
    std::printf("Source     : %d x %d px, %.3f m/px\n",
                src.largeur,
                src.hauteur,
                static_cast<double>(mParPixelSource));
    std::printf("Emprise    : %.0f x %.0f m\n",
                static_cast<double>(carte.largeurM),
                static_cast<double>(carte.hauteurM));
    std::printf("Tuiles     : %d x %d = %d, de %d px a %.3f m/px (%.0f m au sol)\n",
                calage.colonnes,
                calage.rangees,
                total,
                calage.tuilePx,
                static_cast<double>(calage.mParPixel),
                static_cast<double>(tuileM));
    std::printf("Sortie     : %s\n", dossierSortie.string().c_str());
    std::fflush(stdout);

    const dds::Empreinte empreinteSource = dds::empreinteDe(source);
    const float          srcPxParM = static_cast<float>(src.hauteur) / carte.hauteurM;

    std::vector<unsigned char> tuile(static_cast<std::size_t>(tuilePx) *
                                     static_cast<std::size_t>(tuilePx) * 4);
    int           faites = 0;
    int           sautees = 0;
    std::uintmax_t octets = 0;
    for (int rangee = 0; rangee < calage.rangees; ++rangee) {
        for (int col = 0; col < calage.colonnes; ++col) {
            const std::filesystem::path chemin = pyramide.fichier(col, rangee);
            if (reprendre && std::filesystem::exists(chemin)) {
                ++sautees;
                continue;
            }
            composerTuile(src, calage, srcPxParM, col, rangee, tuile);
            const auto blocs = bc7::compresser(tuile.data(), tuilePx, tuilePx, {});
            if (!blocs.has_value()) {
                std::fprintf(stderr, "\nCompression échouée (tuile %d, %d).\n", col, rangee);
                return 1;
            }
            if (!dds::ecrire(chemin, *blocs, empreinteSource)) {
                std::fprintf(stderr, "\nÉcriture échouée : %s\n", chemin.string().c_str());
                return 1;
            }
            std::error_code ec;
            octets += std::filesystem::file_size(chemin, ec);
            ++faites;
            if (faites % 16 == 0 || faites + sautees == total) {
                std::printf("\r  %d / %d tuiles (%.0f Mo)   ",
                            faites + sautees,
                            total,
                            static_cast<double>(octets) / 1e6);
                std::fflush(stdout);
            }
        }
    }

    std::printf("\nTerminé : %d tuiles écrites", faites);
    if (sautees > 0) {
        std::printf(", %d déjà présentes", sautees);
    }
    std::printf(", %.0f Mo au total.\n", static_cast<double>(octets) / 1e6);
    return 0;
}
