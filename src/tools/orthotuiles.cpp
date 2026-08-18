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
 *   ./build/bin/orthotuiles assets/terrain/cauterets /media/disque/cauterets
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "tools/orthotuiles/OrthoModes.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace {

using orthotuiles::CalageCarte;
using orthotuiles::Options;
using orthotuiles::Source;

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

/* Lit la ligne de commande. Faux si une option est inconnue. */
bool lireOptions(int argc, char** argv, Options& opt) {
    opt.dossierCarte  = argv[1];
    opt.dossierSortie = argv[2];
    opt.source        = opt.dossierCarte / "ortho.jpg";

    for (int i = 3; i < argc; ++i) {
        const char* a       = argv[i];
        const bool  aValeur = (i + 1 < argc);
        if (std::strcmp(a, "--m-par-pixel") == 0 && aValeur) {
            opt.mParPixelVoulu = std::strtof(argv[++i], nullptr);
        } else if (std::strcmp(a, "--tuile-px") == 0 && aValeur) {
            opt.tuilePx = std::atoi(argv[++i]);
        } else if (std::strcmp(a, "--source") == 0 && aValeur) {
            opt.source = argv[++i];
        } else if (std::strcmp(a, "--reprendre") == 0) {
            opt.reprendre = true;
        } else if (std::strcmp(a, "--apercu") == 0 && i + 3 < argc) {
            opt.apercuCol     = std::atoi(argv[++i]);
            opt.apercuRangee  = std::atoi(argv[++i]);
            opt.apercuFichier = argv[++i];
        } else if (std::strcmp(a, "--index-seul") == 0) {
            opt.indexSeul = true;
        } else if (std::strcmp(a, "--bloc") == 0 && i + 2 < argc) {
            opt.blocCol    = std::atoi(argv[++i]);
            opt.blocRangee = std::atoi(argv[++i]);
        } else {
            std::fprintf(stderr, "Option inconnue : %s\n", a);
            return false;
        }
    }
    return true;
}

/* Charge l'image source. Rangée 0 au nord, comme l'écrit l'outil de
   préparation des cartes : on ne retourne donc PAS l'image, contrairement au
   chemin de rendu qui doit s'accorder à l'axe vertical d'OpenGL. */
bool chargerSource(const std::filesystem::path& chemin, Source& src) {
    stbi_set_flip_vertically_on_load(0);
    int            canaux = 0;
    unsigned char* pixels = stbi_load(chemin.string().c_str(), &src.largeur, &src.hauteur,
                                      &canaux, 4);
    if (pixels == nullptr) {
        std::fprintf(stderr, "Image source illisible : %s\n", chemin.string().c_str());
        return false;
    }
    src.pixels.assign(pixels, pixels + static_cast<std::size_t>(src.largeur) *
                                           static_cast<std::size_t>(src.hauteur) * 4);
    stbi_image_free(pixels);
    return true;
}

} /* namespace */

int main(int argc, char** argv) {
    if (argc < 3) {
        usage();
        return 1;
    }
    Options opt;
    if (!lireOptions(argc, argv, opt)) {
        usage();
        return 1;
    }

    CalageCarte carte;
    if (!orthotuiles::lireCalageCarte(opt.dossierCarte / "terrain.txt", carte)) {
        std::fprintf(stderr, "Calage de carte illisible : %s\n",
                     (opt.dossierCarte / "terrain.txt").string().c_str());
        return 1;
    }

    /* L'index seul n'a aucune image à lire. */
    if (opt.indexSeul) {
        return orthotuiles::modeIndexSeul(opt, carte);
    }

    Source src;
    if (!chargerSource(opt.source, src)) {
        return 1;
    }

    if (opt.blocCol >= 0 && opt.blocRangee >= 0) {
        return orthotuiles::modeBloc(opt, src);
    }

    /* Finesse de la source : la hauteur au sol divisée par sa hauteur en pixels
       (voir Terrain::orthoMetersPerPixel). */
    const float mParPixelSource = carte.hauteurM / static_cast<float>(src.hauteur);
    float       mParPixel = (opt.mParPixelVoulu > 0.0f) ? opt.mParPixelVoulu : mParPixelSource;
    if (mParPixel < mParPixelSource) {
        std::fprintf(stderr,
                     "Finesse demandée (%.3f m/px) plus fine que la source (%.3f m/px) :\n"
                     "il n'y a pas de détail à y gagner, on reste sur la source.\n",
                     static_cast<double>(mParPixel),
                     static_cast<double>(mParPixelSource));
        mParPixel = mParPixelSource;
    }

    if (!opt.apercuFichier.empty()) {
        return orthotuiles::modeApercu(opt, carte, src, mParPixel);
    }
    return orthotuiles::modeComplet(opt, carte, src, mParPixel);
}
