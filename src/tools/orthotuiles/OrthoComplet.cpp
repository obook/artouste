/*
 * OrthoComplet.cpp
 * Aperçu d'une tuile, et découpage de toute la carte.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "tools/orthotuiles/OrthoModes.hpp"

#include "render/Bc7.hpp"
#include "render/Dds.hpp"
#include "render/tuiles/Pyramide.hpp"

#include <stb_image_write.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <vector>

namespace orthotuiles {

using artouste::render::tuiles::Calage;
using artouste::render::tuiles::Pyramide;
namespace bc7 = artouste::render::bc7;
namespace dds = artouste::render::dds;

int modeApercu(const Options& opt, const CalageCarte& carte, const Source& src, float mParPixel) {
    const Calage calage    = grilleDeCarte(carte, opt.tuilePx, mParPixel);
    const float  srcPxParM = static_cast<float>(src.hauteur) / carte.hauteurM;

    std::vector<unsigned char> apercu(static_cast<std::size_t>(opt.tuilePx) *
                                      static_cast<std::size_t>(opt.tuilePx) * 4);
    composerTuile(src, calage, srcPxParM, opt.apercuCol, opt.apercuRangee, apercu);
    if (stbi_write_png(opt.apercuFichier.string().c_str(),
                       opt.tuilePx,
                       opt.tuilePx,
                       4,
                       apercu.data(),
                       opt.tuilePx * 4) == 0) {
        std::fprintf(stderr, "Écriture de l'aperçu échouée : %s\n",
                     opt.apercuFichier.string().c_str());
        return 1;
    }
    float          coinX = 0.0f, coinZ = 0.0f;
    const Pyramide vue{opt.dossierSortie, calage};
    vue.coinTuile(opt.apercuCol, opt.apercuRangee, coinX, coinZ);
    std::printf("Aperçu de la tuile (%d, %d), coin monde (%.1f, %.1f) : %s\n",
                opt.apercuCol,
                opt.apercuRangee,
                static_cast<double>(coinX),
                static_cast<double>(coinZ),
                opt.apercuFichier.string().c_str());
    return 0;
}

int modeComplet(const Options& opt, const CalageCarte& carte, const Source& src, float mParPixel) {
    const Calage calage = grilleDeCarte(carte, opt.tuilePx, mParPixel);
    if (!calage.valide()) {
        std::fprintf(stderr,
                     "Calage de tuiles inutilisable (tuile_px %d, m/px %.3f, grille %d x %d).\n",
                     calage.tuilePx,
                     static_cast<double>(calage.mParPixel),
                     calage.colonnes,
                     calage.rangees);
        return 1;
    }

    const Pyramide pyramide{opt.dossierSortie, calage};
    if (!pyramide.ecrireIndex()) {
        std::fprintf(stderr,
                     "Impossible d'écrire l'index : %s\n",
                     (opt.dossierSortie / artouste::render::tuiles::NOM_INDEX).string().c_str());
        return 1;
    }

    const int   total           = calage.colonnes * calage.rangees;
    const float mParPixelSource = carte.hauteurM / static_cast<float>(src.hauteur);
    std::printf("Carte      : %s\n", opt.dossierCarte.filename().string().c_str());
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
                static_cast<double>(calage.tuileM()));
    std::printf("Sortie     : %s\n", opt.dossierSortie.string().c_str());
    std::fflush(stdout);

    const dds::Empreinte empreinteSource = dds::empreinteDe(opt.source);
    const float          srcPxParM       = static_cast<float>(src.hauteur) / carte.hauteurM;

    std::vector<unsigned char> tuile(static_cast<std::size_t>(opt.tuilePx) *
                                     static_cast<std::size_t>(opt.tuilePx) * 4);
    int                        faites  = 0;
    int                        sautees = 0;
    std::uintmax_t             octets  = 0;
    for (int rangee = 0; rangee < calage.rangees; ++rangee) {
        for (int col = 0; col < calage.colonnes; ++col) {
            const std::filesystem::path chemin = pyramide.fichier(col, rangee);
            if (opt.reprendre && std::filesystem::exists(chemin)) {
                ++sautees;
                continue;
            }
            composerTuile(src, calage, srcPxParM, col, rangee, tuile);
            const auto blocs = bc7::compresser(tuile.data(), opt.tuilePx, opt.tuilePx, {});
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

} /* namespace orthotuiles */
