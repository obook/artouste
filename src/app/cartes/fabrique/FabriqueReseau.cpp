/*
 * FabriqueReseau.cpp
 * Accès au WMS de l'IGN et repérage des tuiles sans donnée.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "app/cartes/fabrique/FabriqueInterne.hpp"

#include "app/cartes/FabriqueTuiles.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace artouste::app::cartes {

#ifdef ARTOUSTE_HAS_CURL

std::size_t ecrireDansTampon(char* donnees, std::size_t taille, std::size_t nb, void* utilisateur) {
    const std::size_t total = taille * nb;
    auto*             sortie = static_cast<std::vector<unsigned char>*>(utilisateur);
    sortie->insert(sortie->end(), donnees, donnees + total);
    return total;
}

/* Une requête WMS GetMap sur une emprise, en JPEG. Renvoie faux sur échec ; le
   réseau étant capricieux et le service parfois saturé, l'appelant réessaie. */
[[nodiscard]] bool demanderBloc(CURL*                       curl,
                                double                      latLo,
                                double                      lonLo,
                                double                      latHi,
                                double                      lonHi,
                                int                         largeur,
                                int                         hauteur,
                                std::vector<unsigned char>& sortie) {
    char bbox[128];
    std::snprintf(bbox, sizeof(bbox), "%.8f,%.8f,%.8f,%.8f", latLo, lonLo, latHi, lonHi);

    char url[512];
    std::snprintf(url, sizeof(url),
                  "%s?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&LAYERS=%s&STYLES="
                  "&CRS=EPSG:4326&BBOX=%s&WIDTH=%d&HEIGHT=%d&FORMAT=image/jpeg",
                  WMS_URL, WMS_LAYER, bbox, largeur, hauteur);

    sortie.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &ecrireDansTampon);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sortie);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    if (curl_easy_perform(curl) != CURLE_OK) {
        return false;
    }
    /* Le service répond en XML quand il refuse : un JPEG commence par FF D8. */
    return sortie.size() > 2 && sortie[0] == 0xFF && sortie[1] == 0xD8;
}

/* Part de pixels sans donnée (blanc pur) dans une tuile : au-delà, on n'écrit
   pas la tuile et le moteur garde l'orthophoto d'ensemble, recousue à la
   préparation de la carte. Même règle que l'outil de découpage. Sans réseau, il
   n'y a pas de tuile à peser : la fonction reste avec celles qu'elle sert. */
[[nodiscard]] float partBlanche(const std::vector<unsigned char>& tuile) {
    std::size_t       blancs = 0;
    const std::size_t pixels = tuile.size() / 4;
    for (std::size_t i = 0; i < pixels; ++i) {
        if (tuile[i * 4] >= 248 && tuile[i * 4 + 1] >= 248 && tuile[i * 4 + 2] >= 248) {
            ++blancs;
        }
    }
    return (pixels == 0) ? 0.0f : static_cast<float>(blancs) / static_cast<float>(pixels);
}

#endif /* ARTOUSTE_HAS_CURL */

bool reseauDisponible() {
#ifdef ARTOUSTE_HAS_CURL
    return true;
#else
    return false;
#endif
}

} /* namespace artouste::app::cartes */
