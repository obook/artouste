/*
 * FabriqueReliefReseau.cpp
 * Demande d'altitudes au service LiDAR HD de l'IGN, et moyenne des pixels reçus.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/cartes/fabrique/FabriqueReliefInterne.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace artouste::app::cartes {

#ifdef ARTOUSTE_HAS_CURL

namespace {

std::size_t empiler(char* donnees, std::size_t taille, std::size_t nb, void* utilisateur) {
    const std::size_t total  = taille * nb;
    auto*             sortie = static_cast<std::vector<unsigned char>*>(utilisateur);
    sortie->insert(sortie->end(), donnees, donnees + total);
    return total;
}

/* Lit un flottant 32 bits petit-boutien, tel que le BIL les enfile. */
[[nodiscard]] float lireFlottant(const unsigned char* p) noexcept {
    float valeur = 0.0f;
    std::memcpy(&valeur, p, sizeof(valeur));
    return valeur;
}

} /* namespace */

bool demanderAltitudes(CURL* curl, double lonMin, double lonMax, double latMin, double latMax,
                       int nx, int nz, std::vector<float>& sortie,
                       std::uintmax_t& octetsRecus) {
    if (nx < 2 || nz < 2) {
        return false;
    }
    /* Nos points sont des NOEUDS, le premier sur la bordure ouest, alors que le
       service rend des PIXELS dont les centres tombent à un demi-pas à
       l'intérieur : on élargit l'emprise d'un demi-pas de chaque côté. Même
       calage que tools/lidar/services.py, sans quoi tout le jeu glisserait d'un
       demi-pixel. */
    const double pasLon = (lonMax - lonMin) / static_cast<double>(nx - 1);
    const double pasLat = (latMax - latMin) / static_cast<double>(nz - 1);

    char bbox[128];
    std::snprintf(bbox, sizeof(bbox), "%.8f,%.8f,%.8f,%.8f", latMin - 0.5 * pasLat,
                  lonMin - 0.5 * pasLon, latMax + 0.5 * pasLat, lonMax + 0.5 * pasLon);

    const int largeur = nx * RELIEF_SUR_ECH;
    const int hauteur = nz * RELIEF_SUR_ECH;

    char url[512];
    std::snprintf(url, sizeof(url),
                  "%s?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&LAYERS=%s&STYLES="
                  "&CRS=EPSG:4326&BBOX=%s&WIDTH=%d&HEIGHT=%d&FORMAT=%s",
                  WMS_URL, WMS_COUCHE_MNT, bbox, largeur, hauteur, WMS_FORMAT_BIL);

    std::vector<unsigned char> brut;
    brut.reserve(static_cast<std::size_t>(largeur) * static_cast<std::size_t>(hauteur) * 4);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &empiler);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &brut);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    if (curl_easy_perform(curl) != CURLE_OK) {
        return false;
    }
    /* Le service répond en XML quand il refuse : la taille exacte est le seul
       contrôle qui vaille sur des octets bruts. */
    if (brut.size() != static_cast<std::size_t>(largeur) * static_cast<std::size_t>(hauteur) * 4) {
        return false;
    }
    octetsRecus += brut.size();

    /* Moyenne des RELIEF_SUR_ECH x RELIEF_SUR_ECH pixels d'un point : le service
       rééchantillonne au plus proche depuis sa propre pyramide, et un point isolé
       peut tomber sur une arête. Les pixels sans donnée sont écartés de la
       moyenne, la sentinelle étant très négative ; un point dont tous les pixels
       manquent reste sans donnée. */
    sortie.assign(static_cast<std::size_t>(nx) * static_cast<std::size_t>(nz), 0.0f);
    for (int j = 0; j < nz; ++j) {
        for (int i = 0; i < nx; ++i) {
            double somme  = 0.0;
            int    valides = 0;
            for (int dj = 0; dj < RELIEF_SUR_ECH; ++dj) {
                const std::size_t ligne =
                    static_cast<std::size_t>(j * RELIEF_SUR_ECH + dj) *
                    static_cast<std::size_t>(largeur);
                for (int di = 0; di < RELIEF_SUR_ECH; ++di) {
                    const float v = lireFlottant(
                        brut.data() +
                        4 * (ligne + static_cast<std::size_t>(i * RELIEF_SUR_ECH + di)));
                    if (v > RELIEF_NODATA) {
                        somme += static_cast<double>(v);
                        ++valides;
                    }
                }
            }
            sortie[static_cast<std::size_t>(j) * static_cast<std::size_t>(nx) +
                   static_cast<std::size_t>(i)] =
                (valides == 0) ? RELIEF_NODATA - 1.0f
                               : static_cast<float>(somme / static_cast<double>(valides));
        }
    }
    return true;
}

#endif /* ARTOUSTE_HAS_CURL */

} /* namespace artouste::app::cartes */
