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

namespace {

/* Rayons, en mètres, des zones où le relief fin est ramené vers la carte
   d'ensemble : plat au centre, puis fondu jusqu'au relief fin intact.
   Le départ est large : l'aplanissement de flattenPads doit être suivi sans
   marche visible. Un hélipad ordinaire est au plus juste : son plateau ne fait
   que 8 m de rayon, et raboter plus large gommerait le relief fin autour. */
constexpr float PLAT_DEPART_M  = 40.0f;
constexpr float FONDU_DEPART_M = 40.0f;
constexpr float PLAT_PAD_M     = 12.0f;
constexpr float FONDU_PAD_M    = 20.0f;

} /* namespace */

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
    /* Pas de LiDAR sur cette tuile : tout vient de la carte d'ensemble. */
    if (!aDonnee) {
        for (int j = 0; j < cote; ++j) {
            const float z = z0 + static_cast<float>(j) * pasZ;
            for (int i = 0; i < cote; ++i) {
                const float x = x0 + static_cast<float>(i) * pasX;
                hauteurs[static_cast<std::size_t>(j) * static_cast<std::size_t>(cote) +
                         static_cast<std::size_t>(i)] = heightCoarse(x, z);
            }
        }
        return;
    }

    /* Là où le moteur pose quelque chose de plat, le relief fin doit revenir à
       la carte, sinon il passe par-dessus :

       - le DÉPART est aplani dans le maillage d'ensemble (flattenPads) ;
         l'appareil y naîtrait sur une bosse.
       - chaque HÉLIPAD porte une plate-forme calée sur heightAt, donc sur la
         carte d'ensemble (buildPadPlatforms). Le relief fin, lui, est levé au
         LiDAR : mesuré sur toulouse, il dépasse le plateau de 0,18 à 0,45 m
         selon le pad, et le disque se retrouve à moitié enterré. La jupe du
         disque n'y peut rien, elle n'habille que le cas inverse.

       On ne parcourt pas tous les points pour chaque zone : une zone fait
       quelques dizaines de mètres, une tuile plusieurs centaines. On calcule
       donc la fenêtre d'indices couverte et on s'y tient. */
    const auto ramener = [&](float px, float pz, float plat, float fondu) {
        const float rayon = plat + fondu;
        const int   i0 = std::max(0, static_cast<int>(std::ceil((px - rayon - x0) / pasX)));
        const int   i1 = std::min(cote - 1, static_cast<int>(std::floor((px + rayon - x0) / pasX)));
        const int   j0 = std::max(0, static_cast<int>(std::ceil((pz - rayon - z0) / pasZ)));
        const int   j1 = std::min(cote - 1, static_cast<int>(std::floor((pz + rayon - z0) / pasZ)));
        for (int j = j0; j <= j1; ++j) {
            const float z = z0 + static_cast<float>(j) * pasZ;
            for (int i = i0; i <= i1; ++i) {
                const float x = x0 + static_cast<float>(i) * pasX;
                const float d = std::sqrt((x - px) * (x - px) + (z - pz) * (z - pz));
                if (d >= rayon) {
                    continue;
                }
                const float       t = std::clamp((d - plat) / fondu, 0.0f, 1.0f);
                const std::size_t k = static_cast<std::size_t>(j) * static_cast<std::size_t>(cote) +
                                      static_cast<std::size_t>(i);
                /* Deux zones qui se recouvrent (le départ EST un hélipad depuis
                   calerDepartSurHelipad) tirent chacune leur tour : le produit
                   des poids ne fait que ramener davantage vers la carte, jamais
                   l'inverse. */
                hauteurs[k] = heightCoarse(x, z) * (1.0f - t) + hauteurs[k] * t;
            }
        }
    };

    if (m_hasStart) {
        ramener(m_startX, m_startZ, PLAT_DEPART_M, FONDU_DEPART_M);
    }
    for (const PadPlatform& plateau : m_padPlatforms) {
        ramener(plateau.x, plateau.z, PLAT_PAD_M, FONDU_PAD_M);
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
