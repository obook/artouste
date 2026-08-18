/*
 * OrthoBloc.cpp
 * Écriture de l'index seul, et remplissage d'un bloc de la grille.
 *
 * Le pilote de récupération IGN écrit l'index une fois, puis remplit les blocs
 * un par un : la grille ne se recalcule jamais entre deux.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "tools/orthotuiles/OrthoModes.hpp"

#include "render/Bc7.hpp"
#include "render/Dds.hpp"
#include "render/tuiles/Pyramide.hpp"

#include <cstdio>
#include <filesystem>
#include <vector>

namespace orthotuiles {

using artouste::render::tuiles::Calage;
using artouste::render::tuiles::Pyramide;
namespace bc7 = artouste::render::bc7;
namespace dds = artouste::render::dds;

int modeIndexSeul(const Options& opt, const CalageCarte& carte) {
    if (opt.mParPixelVoulu <= 0.0f) {
        std::fprintf(stderr, "--index-seul demande --m-par-pixel.\n");
        return 1;
    }
    const Calage calage = grilleDeCarte(carte, opt.tuilePx, opt.mParPixelVoulu);
    if (!calage.valide()) {
        std::fprintf(stderr, "Calage de tuiles inutilisable.\n");
        return 1;
    }
    const Pyramide pyramide{opt.dossierSortie, calage};
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

int modeBloc(const Options& opt, const Source& src) {
    const auto ouverte = Pyramide::ouvrir(opt.dossierSortie);
    if (!ouverte.has_value()) {
        std::fprintf(stderr,
                     "Index absent ou illisible dans %s : lancer d'abord --index-seul.\n",
                     opt.dossierSortie.string().c_str());
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

    const dds::Empreinte       empreinte = dds::empreinteDe(opt.source);
    std::vector<unsigned char> tuile(static_cast<std::size_t>(calage.tuilePx) *
                                     static_cast<std::size_t>(calage.tuilePx) * 4);
    int                        ecrites  = 0;
    int                        blanches = 0;
    for (int dr = 0; dr < src.hauteur / calage.tuilePx; ++dr) {
        for (int dc = 0; dc < src.largeur / calage.tuilePx; ++dc) {
            const int col    = opt.blocCol + dc;
            const int rangee = opt.blocRangee + dr;
            if (!ouverte->dansGrille(col, rangee)) {
                continue; /* le bloc dépasse la carte : rien à écrire */
            }
            const std::filesystem::path chemin = ouverte->fichier(col, rangee);
            if (opt.reprendre && std::filesystem::exists(chemin)) {
                continue;
            }
            copierTuile(src, calage.tuilePx, dc, dr, tuile);
            if (partBlanche(tuile) > 0.9f) {
                ++blanches;
                continue;
            }
            const auto blocs = bc7::compresser(tuile.data(), calage.tuilePx, calage.tuilePx, {});
            if (!blocs.has_value() || !dds::ecrire(chemin, *blocs, empreinte)) {
                std::fprintf(stderr, "Échec sur la tuile (%d, %d).\n", col, rangee);
                return 1;
            }
            ++ecrites;
        }
    }
    std::printf("Bloc (%d, %d) : %d tuiles écrites", opt.blocCol, opt.blocRangee, ecrites);
    if (blanches > 0) {
        std::printf(", %d hors couverture (laissées à l'orthophoto d'ensemble)", blanches);
    }
    std::printf(".\n");
    return 0;
}

} /* namespace orthotuiles */
