/*
 * FabriqueReliefTuiles.cpp
 * Format d'une tuile de relief, écriture d'un bloc, et relief d'ensemble de la
 * carte servant à boucher les trous du LiDAR.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/cartes/fabrique/FabriqueReliefInterne.hpp"

#include "render/relief/FenetreReliefInterne.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <system_error>
#include <vector>

namespace artouste::app::cartes {

namespace {

/* Écrit un entier 16 bits puis un flottant, petit-boutiens : la lecture du
   moteur fait l'inverse (render/relief/FenetreReliefInterne.hpp). */
void ecrire16(unsigned char* p, std::uint16_t valeur) noexcept {
    p[0] = static_cast<unsigned char>(valeur & 0xFFu);
    p[1] = static_cast<unsigned char>((valeur >> 8) & 0xFFu);
}

void ecrireFlottant(unsigned char* p, float valeur) noexcept {
    std::memcpy(p, &valeur, sizeof(valeur));
}

/* Encode une tuile carrée d'altitudes en mètres, en-tête compris. Le minimum et
   l'étendue sont ceux de CETTE tuile, ce qui donne la quantification la plus
   fine que 16 bits permettent sur son propre dénivelé : 2 cm sur 1300 m. */
void encoderTuile(const std::vector<float>& altitudes, int large, int col, int rangee,
                  float pasX, float pasZ, std::vector<unsigned char>& sortie) {
    const int cote = RELIEF_TUILE_POINTS;
    double    mini = altitudes[static_cast<std::size_t>(rangee * cote) *
                            static_cast<std::size_t>(large) +
                            static_cast<std::size_t>(col * cote)];
    double    maxi = mini;
    for (int j = 0; j < cote; ++j) {
        const std::size_t ligne = static_cast<std::size_t>(rangee * cote + j) *
                                      static_cast<std::size_t>(large) +
                                  static_cast<std::size_t>(col * cote);
        for (int i = 0; i < cote; ++i) {
            const double v = altitudes[ligne + static_cast<std::size_t>(i)];
            mini           = std::min(mini, v);
            maxi           = std::max(maxi, v);
        }
    }
    /* Une tuile parfaitement plate donnerait une étendue nulle et une division
       par zéro ; le plancher ne coûte rien, tous les niveaux valant alors 0. */
    const double etendue = std::max(maxi - mini, 1e-3);

    sortie.assign(render::relief::EN_TETE_V2_OCTETS +
                      static_cast<std::size_t>(cote) * static_cast<std::size_t>(cote) * 2,
                  0);
    std::memcpy(sortie.data(), render::relief::MAGIQUE, sizeof(render::relief::MAGIQUE));
    ecrire16(sortie.data() + 4, 2); /* version 2 : un pas par axe */
    ecrire16(sortie.data() + 6, static_cast<std::uint16_t>(cote));
    ecrireFlottant(sortie.data() + 8, pasX);
    ecrireFlottant(sortie.data() + 12, pasZ);
    ecrireFlottant(sortie.data() + 16, static_cast<float>(mini));
    ecrireFlottant(sortie.data() + 20, static_cast<float>(etendue));

    unsigned char* niveaux = sortie.data() + render::relief::EN_TETE_V2_OCTETS;
    for (int j = 0; j < cote; ++j) {
        const std::size_t ligne = static_cast<std::size_t>(rangee * cote + j) *
                                      static_cast<std::size_t>(large) +
                                  static_cast<std::size_t>(col * cote);
        for (int i = 0; i < cote; ++i) {
            const double part =
                (static_cast<double>(altitudes[ligne + static_cast<std::size_t>(i)]) - mini) /
                etendue * 65535.0;
            const long niveau = std::lround(std::clamp(part, 0.0, 65535.0));
            ecrire16(niveaux + 2 * (static_cast<std::size_t>(j) * static_cast<std::size_t>(cote) +
                                    static_cast<std::size_t>(i)),
                     static_cast<std::uint16_t>(niveau));
        }
    }
}

} /* namespace */

int ecrireBlocRelief(const std::filesystem::path& sortie, const std::vector<float>& altitudes,
                     const std::vector<unsigned char>& manquant, int col0, int rangee0, int nCol,
                     int nRangee, float pasX, float pasZ, std::uintmax_t& octetsEcrits) {
    const int                  cote  = RELIEF_TUILE_POINTS;
    const int                  large = nCol * cote;
    std::vector<unsigned char> tuile;
    int                        ecrites = 0;

    for (int r = 0; r < nRangee; ++r) {
        for (int c = 0; c < nCol; ++c) {
            bool aDonnee = false;
            for (int j = 0; j < cote && !aDonnee; ++j) {
                const std::size_t ligne = static_cast<std::size_t>(r * cote + j) *
                                              static_cast<std::size_t>(large) +
                                          static_cast<std::size_t>(c * cote);
                for (int i = 0; i < cote; ++i) {
                    if (manquant[ligne + static_cast<std::size_t>(i)] == 0) {
                        aDonnee = true;
                        break;
                    }
                }
            }
            /* Tuile entièrement hors couverture LiDAR : on ne l'écrit pas, le
               moteur y garde le relief d'ensemble. */
            if (!aDonnee) {
                continue;
            }
            encoderTuile(altitudes, large, c, r, pasX, pasZ, tuile);

            std::error_code ec;
            std::filesystem::create_directories(sortie / std::to_string(rangee0 + r), ec);
            const std::filesystem::path chemin = sortie / std::to_string(rangee0 + r) /
                                                 (std::to_string(col0 + c) + ".r16");
            std::ofstream out(chemin, std::ios::binary | std::ios::trunc);
            if (!out) {
                return -1; /* droits refusés : le bloc n'est pas fait */
            }
            out.write(reinterpret_cast<const char*>(tuile.data()),
                      static_cast<std::streamsize>(tuile.size()));
            /* Fermeture explicite, et contrôle APRÈS elle : un disque plein ne se
               voit qu'au vidage du tampon, et une tuile tronquée passée pour
               écrite serait marquée faite, donc jamais reprise. */
            out.close();
            if (!out) {
                return -1;
            }
            octetsEcrits += tuile.size();
            ++ecrites;
        }
    }
    return ecrites;
}

std::filesystem::path cheminMarqueBloc(const std::filesystem::path& sortie, int col0,
                                       int rangee0) {
    return sortie / ".blocs" / (std::to_string(col0) + "_" + std::to_string(rangee0));
}

bool ReliefCarte::charger(const std::filesystem::path& dossierCarte, const CalageCarte& carte) {
    /* Rangée 0 au nord, comme l'a écrite l'outil de préparation. */
    stbi_set_flip_vertically_on_load(0);
    int             canaux = 0;
    unsigned short* pixels = stbi_load_16((dossierCarte / "heightmap.png").string().c_str(),
                                          &m_colonnes, &m_rangees, &canaux, 1);
    if (pixels == nullptr || m_colonnes < 2 || m_rangees < 2) {
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return false;
    }
    m_niveaux.assign(pixels, pixels + static_cast<std::size_t>(m_colonnes) *
                                          static_cast<std::size_t>(m_rangees));
    stbi_image_free(pixels);
    m_carte = carte;
    return m_carte.elevMax > 0.0f;
}

float ReliefCarte::altitude(double lon, double lat) const noexcept {
    const double lonMin     = m_carte.lonMin;
    const double latMax     = m_carte.latMax;
    const double etendueLon = m_carte.lonMax - lonMin;
    const double etendueLat = latMax - m_carte.latMin;
    const double i =
        std::round((lon - lonMin) / etendueLon * static_cast<double>(m_colonnes - 1));
    const double j =
        std::round((latMax - lat) / etendueLat * static_cast<double>(m_rangees - 1));
    const std::size_t col =
        static_cast<std::size_t>(std::clamp(i, 0.0, static_cast<double>(m_colonnes - 1)));
    const std::size_t rangee =
        static_cast<std::size_t>(std::clamp(j, 0.0, static_cast<double>(m_rangees - 1)));
    const std::size_t k = rangee * static_cast<std::size_t>(m_colonnes) + col;
    return static_cast<float>(static_cast<double>(m_niveaux[k]) / 65535.0 *
                              static_cast<double>(m_carte.elevMax));
}

} /* namespace artouste::app::cartes */
