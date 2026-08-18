/*
 * BuildingsEmprise.cpp
 * Emprises à ne pas extruder (voir BuildingsEmprise.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/buildings/BuildingsEmprise.hpp"

#include "render/Terrain.hpp"

#include <cstddef>

namespace artouste::render {

namespace {

/* Rayon dégagé autour d'un hélipad, au carré. */
constexpr float PAD_CLEAR_M2 = 15.0f * 15.0f;

/* Point dans le polygone, par lancer de rayon. */
[[nodiscard]] bool dansEmprise(const std::vector<float>& px,
                               const std::vector<float>& pz,
                               float                     X,
                               float                     Z) {
    const std::size_t n      = px.size();
    bool              dedans = false;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((pz[i] > Z) != (pz[j] > Z)) &&
            (X < (px[j] - px[i]) * (Z - pz[i]) / (pz[j] - pz[i]) + px[i])) {
            dedans = !dedans;
        }
    }
    return dedans;
}

} /* namespace */

Degagements::Degagements(const Terrain& terrain) {
    if (terrain.hasStart()) {
        m_pads.push_back(Zone{terrain.startX(), terrain.startZ(), PAD_CLEAR_M2});
    }
    for (const Landmark& pad : terrain.helipads()) {
        Zone zone;
        terrain.worldAt(pad.lon, pad.lat, zone.x, zone.z);
        zone.rayon2 = PAD_CLEAR_M2;
        m_pads.push_back(zone);
    }
    for (const Monument& mon : terrain.monuments()) {
        if (mon.clearRadiusM <= 0.0f) {
            continue; /* monument sans dégagement demandé : les bâtiments restent */
        }
        Zone zone;
        terrain.worldAt(mon.lon, mon.lat, zone.x, zone.z);
        zone.rayon2 = mon.clearRadiusM * mon.clearRadiusM;
        m_monuments.push_back(zone);
    }
}

bool Degagements::surUnPad(const std::vector<float>& px,
                           const std::vector<float>& pz,
                           float                     cx,
                           float                     cz) const {
    for (const Zone& pad : m_pads) {
        if (dansEmprise(px, pz, pad.x, pad.z)) {
            return true;
        }
        const float dcx = cx - pad.x;
        const float dcz = cz - pad.z;
        if (dcx * dcx + dcz * dcz < pad.rayon2) {
            return true;
        }
        for (std::size_t k = 0; k < px.size(); ++k) {
            const float ddx = px[k] - pad.x;
            const float ddz = pz[k] - pad.z;
            if (ddx * ddx + ddz * ddz < pad.rayon2) {
                return true;
            }
        }
    }
    return false;
}

bool Degagements::sousUnMonument(const std::vector<float>& px,
                                 const std::vector<float>& pz,
                                 float                     cx,
                                 float                     cz) const {
    for (const Zone& mon : m_monuments) {
        const float dmx = cx - mon.x;
        const float dmz = cz - mon.z;
        /* Le test point-dans-polygone rattrape la grande emprise qui contient
           le monument sans que son centre en soit proche : l'esplanade du
           Champ de Mars autour de la tour. */
        if (dmx * dmx + dmz * dmz < mon.rayon2 || dansEmprise(px, pz, mon.x, mon.z)) {
            return true;
        }
    }
    return false;
}

} /* namespace artouste::render */
