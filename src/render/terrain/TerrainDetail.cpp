/*
 * TerrainDetail.cpp
 * Fenêtres de détail et de relief : ouverture, suivi, raccord au socle.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/Terrain.hpp"

#include "render/TextureCache.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

namespace artouste::render {

void Terrain::ouvrirDetail(const std::filesystem::path& dir, int fenetrePx,
                           const std::filesystem::path& racineTuiles) {
    if (fenetrePx <= 0) {
        return;  /* détail fin refusé par la configuration */
    }
    const std::filesystem::path candidat = tuiles::cheminJeuDeTuiles(dir, racineTuiles);
    if (!candidat.empty()) {
        std::vector<tuiles::Pyramide> niveaux = tuiles::ouvrirNiveaux(candidat);

        /* Des tuiles pas plus fines que l'orthophoto déjà chargée n'apporteraient
           rien et coûteraient de la mémoire vidéo : c'est le cas d'un jeu de
           tuiles produit à la finesse de la source pour une carte dont
           l'orthophoto d'ensemble est déjà fine. */
        const float finesseOrtho = orthoMetersPerPixel();
        if (finesseOrtho > 0.0f) {
            const std::size_t avant = niveaux.size();
            std::erase_if(niveaux, [finesseOrtho](const tuiles::Pyramide& p) {
                return !tuiles::niveauUtile(p.calage().mParPixel, finesseOrtho);
            });
            if (niveaux.empty()) {
                std::printf("[tuiles] %s : aucun niveau plus fin que l'orthophoto "
                            "(%.2f m/px), fenêtre inutile.\n",
                            candidat.string().c_str(),
                            static_cast<double>(finesseOrtho));
                return;
            }
            if (niveaux.size() != avant) {
                std::printf("[tuiles] %s : %zu niveau(x) écarté(s), pas plus fin(s) que "
                            "l'orthophoto.\n",
                            candidat.string().c_str(),
                            avant - niveaux.size());
            }
        }

        /* Deux niveaux au plus, les DEUX PLUS FINS : au-delà, l'orthophoto
           d'ensemble couvre déjà le lointain, et chaque fenêtre supplémentaire
           coûterait sa mémoire vidéo pour une distance où l'oeil ne distingue
           plus rien. Ils sont classés du plus large au plus fin, on garde donc
           la fin de la liste. */
        if (niveaux.size() > 2) {
            niveaux.erase(niveaux.begin(),
                          niveaux.end() - 2);
        }

        /* La fenêtre serrée est deux fois plus petite que la large : elle ne sert
           qu'au ras du sol, où son rayon suffit largement, et la seconde moitié
           de sa mémoire vidéo serait dépensée pour du terrain que le niveau
           large couvre déjà correctement. */
        m_detail = std::make_unique<tuiles::Fenetre>(std::move(niveaux.front()), fenetrePx);
        if (!m_detail->active()) {
            m_detail.reset();
            return;
        }
        if (niveaux.size() > 1) {
            auto serree =
                std::make_unique<tuiles::Fenetre>(std::move(niveaux.back()),
                                                  std::max(1024, fenetrePx / 2));
            if (serree->active()) {
                m_detailFin = std::move(serree);
            }
        }
        return;
    }
}

void Terrain::ouvrirRelief(const std::filesystem::path& dir,
                           const std::filesystem::path& racineTuiles) {
    const std::filesystem::path candidat = relief::cheminJeuDeRelief(dir, racineTuiles);
    if (candidat.empty() || m_heights.empty()) {
        return;
    }

    /* Relief d'ensemble en texture, tel qu'il est après aplanissement du départ :
       la fenêtre s'y raccorde au bord, et une tuile absente y ramène. */
    glGenTextures(1, &m_carteRelief);
    glBindTexture(GL_TEXTURE_2D, m_carteRelief);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_cols, m_rows, 0, GL_RED, GL_FLOAT,
                 m_heights.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_relief = relief::FenetreRelief::ouvrir(
        candidat,
        [this](float x0, float z0, float pasX, float pasZ, int cote, float* hauteurs,
               bool aDonnee) {
            corrigerTuileRelief(x0, z0, pasX, pasZ, cote, hauteurs, aDonnee);
        },
        relief::COTE_GRILLE_POINTS);
    if (!m_relief) {
        glDeleteTextures(1, &m_carteRelief);
        m_carteRelief = 0;
        return;
    }

    /* Contrôle d'emboîtement. Un jeu de tuiles dont le pas ne divise pas la
       maille de la carte fait redessiner au lieu de reproduire, et les
       silhouettes bougent à la frontière de la fenêtre. On le dit au chargement
       plutôt que de le laisser découvrir en vol. */
    const float dx = m_widthM / static_cast<float>(m_cols - 1);
    const float dz = m_heightM / static_cast<float>(m_rows - 1);
    const bool  ok = relief::emboiteDansMaille(m_relief->pasX(), dx, relief::PAS_ANNEAU) &&
                    relief::emboiteDansMaille(m_relief->pasZ(), dz, relief::PAS_ANNEAU);
    std::printf("[relief] emboîtement dans la carte (%.4f x %.4f m) : %s.\n",
                static_cast<double>(dx), static_cast<double>(dz),
                ok ? "oui" : "NON, la frontière se verra");
}

void Terrain::corrigerTuileRelief(float x0, float z0, float pasX, float pasZ, int cote,
                                  float* hauteurs, bool aDonnee) const noexcept {
    /* Le départ est aplani dans le maillage d'ensemble (voir flattenPads) : le
       relief fin doit y revenir, sinon l'appareil naîtrait sur une bosse. */
    constexpr float PLAT_M  = 40.0f;
    constexpr float FONDU_M = 40.0f;
    const float     demiX   = 0.5f * static_cast<float>(cote) * pasX;
    const float     demiZ   = 0.5f * static_cast<float>(cote) * pasZ;
    const bool      proche  = m_hasStart &&
                        std::fabs(x0 + demiX - m_startX) < demiX + PLAT_M + FONDU_M &&
                        std::fabs(z0 + demiZ - m_startZ) < demiZ + PLAT_M + FONDU_M;
    if (aDonnee && !proche) {
        return;
    }

    for (int j = 0; j < cote; ++j) {
        const float z = z0 + static_cast<float>(j) * pasZ;
        for (int i = 0; i < cote; ++i) {
            const float       x = x0 + static_cast<float>(i) * pasX;
            const std::size_t k = static_cast<std::size_t>(j) * static_cast<std::size_t>(cote) +
                                  static_cast<std::size_t>(i);
            if (!aDonnee) {
                hauteurs[k] = heightCoarse(x, z);
                continue;
            }
            const float d = std::sqrt((x - m_startX) * (x - m_startX) +
                                      (z - m_startZ) * (z - m_startZ));
            if (d >= PLAT_M + FONDU_M) {
                continue;
            }
            const float t = std::clamp((d - PLAT_M) / FONDU_M, 0.0f, 1.0f);
            hauteurs[k]   = heightCoarse(x, z) * (1.0f - t) + hauteurs[k] * t;
        }
    }
}

void Terrain::suivreDetail(float x, float z, float dt) {
    if (m_detail) {
        m_detail->suivre(x, z, dt);
    }
    if (m_detailFin) {
        m_detailFin->suivre(x, z, dt);
    }
    if (m_relief) {
        /* ARTOUSTE_DEBUG_RELIEF_DECALAGE : avance la fenêtre seule, caméra
           immobile, pour faire tomber une paroi du côté de l'anneau. À
           retirer. */
        static const float decalage = [] {
            const char* e = std::getenv("ARTOUSTE_DEBUG_RELIEF_DECALAGE");
            return (e != nullptr) ? std::strtof(e, nullptr) : 0.0f;
        }();
        m_relief->suivre(x + decalage, z);
    }
}

} /* namespace artouste::render */
