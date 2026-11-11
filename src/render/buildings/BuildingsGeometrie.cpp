/*
 * BuildingsGeometrie.cpp
 * Extrusion d'une emprise (voir BuildingsGeometrie.hpp).
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/buildings/BuildingsGeometrie.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace artouste::render {

namespace {

/* Taille réelle d'une case de l'atlas de façades (assets/textures/facade.png,
   mêmes valeurs dans tools/facade/generer_facade.py). Les UV des murs sont en
   mètres réels divisés par ces tailles : la texture se pose à la même échelle
   sur un pavillon et sur un immeuble, sans dépendre du nombre de sommets. En
   hauteur, une case est un étage, et le mur en reçoit un nombre entier : la
   hauteur réelle est étirée au besoin, pour que la gouttière ne coupe jamais
   une fenêtre.
   La colonne de l'atlas voyage dans l'UV vertical, par pas de FACADE_PAS_UV :
   la structure Vertex est partagée avec le terrain, on n'y ajoute pas
   d'attribut. building.frag la retranche. */
constexpr float FACADE_TILE_W_M = 12.0f;
constexpr float FACADE_TILE_H_M = 3.0f;
constexpr float FACADE_PAS_UV   = 100.0f;

/* Taille réelle d'une case de la texture de couverture des toits
   (assets/textures/couverture.png, tools/facade/generer_couverture.py). */
constexpr float COUVERTURE_TILE_M = 2.0f;

/* Terrasse d'un immeuble : l'édicule d'ascenseur et les climatiseurs. */
constexpr float EDICULE_DEMI_L_M = 1.5f;
constexpr float EDICULE_DEMI_W_M = 1.25f;
constexpr float EDICULE_HAUT_M   = 2.6f;
constexpr float CLIM_DEMI_M      = 0.6f;
constexpr float CLIM_HAUT_M      = 0.8f;
constexpr int   CLIM_MAX         = 2;

/* Au-delà de 45 degrés, la face ne regarde plus dans l'axe de référence. */
constexpr float COS_45 = 0.70710678f;

void pushTriangle(std::vector<Vertex>&       verts,
                  std::vector<unsigned int>& idx,
                  const vec3&                a,
                  const vec3&                b,
                  const vec3&                c,
                  const vec3&                normal,
                  const vec3&                color) {
    /* UV en mètres monde : la couverture se répète au sol, sans couture d'un
       triangle à l'autre. */
    const auto uvDe = [](const vec3& p) {
        return vec2{p.x / COUVERTURE_TILE_M, p.z / COUVERTURE_TILE_M};
    };
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{a, normal, color, uvDe(a)});
    verts.push_back(Vertex{b, normal, color, uvDe(b)});
    verts.push_back(Vertex{c, normal, color, uvDe(c)});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
}

/* Quad d'un mur (bi, bj en bas ; ti, tj en haut, pas forcément à la même
   hauteur sous un toit en pente). Les UV vont de u0 à u1 en largeur et de 0 à
   vi, vj en hauteur, v0 en bas (la colonne de l'atlas, voir FACADE_PAS_UV).
   Des u négatifs demandent la tuile aveugle plutôt que la tuile fenêtrée ; la
   répétition, elle, est indifférente au signe. */
void pushWallQuad(std::vector<Vertex>&       verts,
                  std::vector<unsigned int>& idx,
                  const vec3&                bi,
                  const vec3&                bj,
                  const vec3&                tj,
                  const vec3&                ti,
                  const vec3&                normal,
                  const vec3&                color,
                  float                      u0,
                  float                      u1,
                  float                      vi,
                  float                      vj,
                  float                      v0) {
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{bi, normal, color, {u0, v0}});
    verts.push_back(Vertex{bj, normal, color, {u1, v0}});
    verts.push_back(Vertex{tj, normal, color, {u1, vj}});
    verts.push_back(Vertex{ti, normal, color, {u0, vi}});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
}

/* Normale du côté le plus long de l'emprise. Elle sert d'axe de référence pour
   décider quelles faces sont fenêtrées : un bâtiment réel montre ses fenêtres
   sur ses deux longues faces et garde ses pignons pleins. */
[[nodiscard]] vec3 normaleDeReference(const std::vector<float>& px,
                                      const std::vector<float>& pz) {
    const std::size_t n       = px.size();
    float             refNx   = 1.0f;
    float             refNz   = 0.0f;
    float             refLen2 = -1.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j    = (i + 1) % n;
        const float       ex   = px[j] - px[i];
        const float       ez   = pz[j] - pz[i];
        const float       len2 = ex * ex + ez * ez;
        if (len2 > refLen2) {
            refLen2         = len2;
            const float inv = 1.0f / std::sqrt(std::max(len2, 1e-6f));
            refNx           = ez * inv;
            refNz           = -ex * inv;
        }
    }
    return vec3{refNx, 0.0f, refNz};
}

/* Quad quelconque, avec sa normale et ses UV (nuls par défaut : le tablier,
   dont aucune face n'est un mur de façade ; un pan de toit donne les siens). */
void pushQuad(std::vector<Vertex>&       verts,
              std::vector<unsigned int>& idx,
              const vec3&                a,
              const vec3&                b,
              const vec3&                c,
              const vec3&                d,
              const vec3&                normal,
              const vec3&                color,
              const vec2&                uvA = {0.0f, 0.0f},
              const vec2&                uvB = {0.0f, 0.0f},
              const vec2&                uvC = {0.0f, 0.0f},
              const vec2&                uvD = {0.0f, 0.0f}) {
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{a, normal, color, uvA});
    verts.push_back(Vertex{b, normal, color, uvB});
    verts.push_back(Vertex{c, normal, color, uvC});
    verts.push_back(Vertex{d, normal, color, uvD});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
}

/* Perpendiculaire horizontale à l'axe au point i. Aux points intérieurs on
   moyenne les deux segments voisins : sans cela le ruban montrerait une fente
   à chaque changement de direction. */
[[nodiscard]] vec3 perpendiculaireAxe(const std::vector<float>& px,
                                      const std::vector<float>& pz,
                                      std::size_t               i) {
    const std::size_t n = px.size();
    float             dx = 0.0f;
    float             dz = 0.0f;
    const auto ajouter = [&](std::size_t a, std::size_t b) {
        const float ex   = px[b] - px[a];
        const float ez   = pz[b] - pz[a];
        const float len2 = ex * ex + ez * ez;
        if (len2 > 1e-12f) {
            const float inv = 1.0f / std::sqrt(len2);
            dx += ex * inv;
            dz += ez * inv;
        }
    };
    if (i > 0) {
        ajouter(i - 1, i);
    }
    if (i + 1 < n) {
        ajouter(i, i + 1);
    }
    const float len2 = dx * dx + dz * dz;
    if (len2 < 1e-12f) {
        return vec3{1.0f, 0.0f, 0.0f}; /* axe dégénéré : direction arbitraire */
    }
    const float inv = 1.0f / std::sqrt(len2);
    return vec3{dz * inv, 0.0f, -dx * inv};
}

/* Toit à deux pans. Seules les emprises proches de leur rectangle (au moins
   RECTANGULARITE_MIN de sa surface) et de taille de maison ou de grange le
   reçoivent : sur un immeuble ou une forme tordue, un faîtage unique serait
   faux, on y garde le toit plat. */
constexpr float RECTANGULARITE_MIN = 0.85f;
constexpr float SURFACE_DEUX_PANS_MAX_M2 = 600.0f;

/* En dessous, la BD TOPO décrit un toit plat (terrasse, hangar). Une hauteur
   négative veut dire inconnue : on suppose alors un toit en pente, le cas le
   plus courant. */
constexpr float TOIT_PLAT_M = 0.5f;

/* La hauteur de toit BD TOPO sous-estime souvent la pente (médiane 1,9 m à
   Laruns, là où l'ardoise monte à 45 degrés) : on impose un minimum selon le
   matériau, et un maximum pour que le shader y voie encore un toit (n.y >= 0,5,
   soit 60 degrés). Valeurs en tangente de la pente. */
constexpr float TAN_PENTE_ARDOISE = 0.8391f; /* 40 degrés */
constexpr float TAN_PENTE_AUTRE   = 0.5774f; /* 30 degrés */
constexpr float TAN_PENTE_MAX     = 1.4281f; /* 55 degrés */

/* Avant-toit : le toit déborde des murs de cette distance, sur les quatre
   côtés. C'est lui qui donne sa silhouette à une maison. */
constexpr float DEBORD_M = 0.4f;

/* Rectangle orienté : centre (cx, cz), axe long u (unitaire), axe court v
   (perpendiculaire), demi-longueur a >= demi-largeur b. */
struct Rectangle {
    float cx, cz, ux, uz, vx, vz, a, b;
};

/* Plus petit rectangle contenant l'emprise. Un de ses côtés porte toujours un
   côté de l'enveloppe convexe : il suffit d'essayer la direction de chaque côté
   de l'emprise. */
[[nodiscard]] Rectangle rectangleMinimal(const std::vector<float>& px,
                                         const std::vector<float>& pz) {
    const std::size_t n = px.size();
    Rectangle         meilleur{px[0], pz[0], 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    float             surfaceMin = 1e30f;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j   = (i + 1) % n;
        const float       ex  = px[j] - px[i];
        const float       ez  = pz[j] - pz[i];
        const float       len = std::sqrt(ex * ex + ez * ez);
        if (len < 1e-3f) {
            continue;
        }
        const float ux = ex / len, uz = ez / len;
        const float vx = -uz, vz = ux;
        float       minU = 1e30f, maxU = -1e30f, minV = 1e30f, maxV = -1e30f;
        for (std::size_t k = 0; k < n; ++k) {
            /* Relatif au sommet 0 : les coordonnées monde dépassent 10 km. */
            const float dx = px[k] - px[0], dz = pz[k] - pz[0];
            const float pu = dx * ux + dz * uz, pv = dx * vx + dz * vz;
            minU = std::min(minU, pu);
            maxU = std::max(maxU, pu);
            minV = std::min(minV, pv);
            maxV = std::max(maxV, pv);
        }
        const float surface = (maxU - minU) * (maxV - minV);
        if (surface < surfaceMin) {
            surfaceMin      = surface;
            const float mu  = 0.5f * (minU + maxU);
            const float mv  = 0.5f * (minV + maxV);
            meilleur        = Rectangle{px[0] + mu * ux + mv * vx, pz[0] + mu * uz + mv * vz,
                                        ux, uz, vx, vz, 0.5f * (maxU - minU), 0.5f * (maxV - minV)};
        }
    }
    if (meilleur.a < meilleur.b) { /* le faîtage suit le grand côté */
        meilleur = Rectangle{meilleur.cx, meilleur.cz, meilleur.vx, meilleur.vz,
                             -meilleur.ux, -meilleur.uz, meilleur.b, meilleur.a};
    }
    return meilleur;
}

/* Surface signée de l'emprise (formule du lacet) : positive dans un sens de
   parcours, négative dans l'autre. */
[[nodiscard]] float surfaceSignee(const std::vector<float>& px, const std::vector<float>& pz) {
    float s = 0.0f;
    for (std::size_t i = 0; i < px.size(); ++i) {
        const std::size_t j = (i + 1) % px.size();
        s += (px[i] - px[0]) * (pz[j] - pz[0]) - (px[j] - px[0]) * (pz[i] - pz[0]);
    }
    return 0.5f * s;
}

/* Triangulation par oreilles : on retire un à un les sommets convexes dont le
   triangle ne contient aucun autre sommet. Contrairement à l'éventail, elle
   reste juste sur une emprise concave (maison en L, en U). Renvoie les indices,
   trois par triangle. */
[[nodiscard]] std::vector<std::size_t> trianguler(const std::vector<float>& px,
                                                  const std::vector<float>& pz) {
    std::vector<std::size_t> reste(px.size());
    for (std::size_t k = 0; k < reste.size(); ++k) {
        reste[k] = k;
    }
    const float sens = surfaceSignee(px, pz) >= 0.0f ? 1.0f : -1.0f;
    const auto  croix = [&](std::size_t a, std::size_t b, std::size_t c) {
        return sens * ((px[b] - px[a]) * (pz[c] - pz[a]) - (px[c] - px[a]) * (pz[b] - pz[a]));
    };

    std::vector<std::size_t> tris;
    while (reste.size() > 3) {
        const std::size_t m      = reste.size();
        bool              trouve = false;
        for (std::size_t k = 0; k < m && !trouve; ++k) {
            const std::size_t a = reste[(k + m - 1) % m];
            const std::size_t b = reste[k];
            const std::size_t c = reste[(k + 1) % m];
            const float       z = croix(a, b, c);
            if (std::fabs(z) < 1e-4f) { /* sommet aligné : on le retire sans triangle */
                reste.erase(reste.begin() + static_cast<std::ptrdiff_t>(k));
                trouve = true;
                break;
            }
            if (z < 0.0f) {
                continue; /* sommet rentrant */
            }
            bool vide = true;
            for (const std::size_t p : reste) {
                if (p != a && p != b && p != c && croix(a, b, p) > 0.0f &&
                    croix(b, c, p) > 0.0f && croix(c, a, p) > 0.0f) {
                    vide = false;
                    break;
                }
            }
            if (vide) {
                tris.insert(tris.end(), {a, b, c});
                reste.erase(reste.begin() + static_cast<std::ptrdiff_t>(k));
                trouve = true;
            }
        }
        if (!trouve) {
            break; /* emprise qui se recoupe : on finit en éventail */
        }
    }
    for (std::size_t k = 1; k + 1 < reste.size(); ++k) {
        tris.insert(tris.end(), {reste[0], reste[k], reste[k + 1]});
    }
    return tris;
}

/* Point dans l'emprise (lancer de rayon, parité des croisements). */
[[nodiscard]] bool dansEmprise(const std::vector<float>& px, const std::vector<float>& pz,
                               float x, float z) {
    bool dedans = false;
    for (std::size_t i = 0, j = px.size() - 1; i < px.size(); j = i++) {
        if ((pz[i] > z) != (pz[j] > z) &&
            x < px[j] + (px[i] - px[j]) * (z - pz[j]) / (pz[i] - pz[j])) {
            dedans = !dedans;
        }
    }
    return dedans;
}

/* Boîte posée sur la terrasse : centre (cx, cz), axes (ux, uz) et sa
   perpendiculaire, demi-côtés a et b, du niveau bas au niveau haut. Murs
   aveugles de la colonne facade, dessus en gravier comme la terrasse. Rien n'est
   posé si un coin sort de l'emprise. Renvoie vrai si la boîte a été posée. */
bool poserBoite(std::vector<Vertex>&       verts,
                std::vector<unsigned int>& idx,
                const std::vector<float>&  px,
                const std::vector<float>&  pz,
                float cx, float cz, float ux, float uz, float a, float b,
                float bas, float haut, int facade, const vec3& mur, const vec3& toit) {
    const float vx = -uz, vz = ux;
    const vec3  coin[4] = {{cx - a * ux - b * vx, bas, cz - a * uz - b * vz},
                           {cx + a * ux - b * vx, bas, cz + a * uz - b * vz},
                           {cx + a * ux + b * vx, bas, cz + a * uz + b * vz},
                           {cx - a * ux + b * vx, bas, cz - a * uz + b * vz}};
    for (const vec3& c : coin) {
        if (!dansEmprise(px, pz, c.x, c.z)) {
            return false;
        }
    }
    const float v = static_cast<float>(facade) * FACADE_PAS_UV + 1.0f; /* un étage */
    for (int i = 0; i < 4; ++i) {
        const vec3& p = coin[i];
        const vec3& q = coin[(i + 1) % 4];
        const vec3  normal = glm::normalize(vec3{q.z - p.z, 0.0f, -(q.x - p.x)});
        const float u1     = -glm::length(vec2{q.x - p.x, q.z - p.z}) / FACADE_TILE_W_M;
        pushWallQuad(verts, idx, p, q, vec3{q.x, haut, q.z}, vec3{p.x, haut, p.z}, normal, mur,
                     -1e-4f, u1, v, v, v - 1.0f);
    }
    const vec3 up{0.0f, 1.0f, 0.0f};
    pushTriangle(verts, idx, vec3{coin[0].x, haut, coin[0].z}, vec3{coin[2].x, haut, coin[2].z},
                 vec3{coin[1].x, haut, coin[1].z}, up, toit);
    pushTriangle(verts, idx, vec3{coin[0].x, haut, coin[0].z}, vec3{coin[3].x, haut, coin[3].z},
                 vec3{coin[2].x, haut, coin[2].z}, up, toit);
    return true;
}

} /* namespace */

float extruderBatiment(std::vector<Vertex>&       verts,
                       std::vector<unsigned int>& idx,
                       const std::vector<float>&  px,
                       const std::vector<float>&  pz,
                       float                      base,
                       float                      hauteur,
                       float                      hauteurToit,
                       bool                       ardoise,
                       int                        facade,
                       const vec3&                mur,
                       const vec3&                toit) {
    const std::size_t n   = px.size();
    const float       top = base + hauteur;

    /* Nombre entier d'étages sur la hauteur de gouttière, un au moins ; l'UV
       vertical porte aussi la colonne de l'atlas. */
    const float etages = std::max(1.0f, std::round(hauteur / FACADE_TILE_H_M));
    const float vParM  = etages / std::max(hauteur, 0.1f);
    const float vBase  = static_cast<float>(facade) * FACADE_PAS_UV;

    /* Deux pans ou toit plat. tanPente reste nul pour un toit plat : la hauteur
       du toit vaut alors top partout, et tout ce qui suit marche pareil. */
    const Rectangle r       = rectangleMinimal(px, pz);
    const float     signee  = surfaceSignee(px, pz);
    const float     surface = std::fabs(signee);
    const bool      deuxPans = (hauteurToit < 0.0f || hauteurToit >= TOIT_PLAT_M) &&
                          hauteur < HAUTEUR_IMMEUBLE_M && surface <= SURFACE_DEUX_PANS_MAX_M2 &&
                          surface >= RECTANGULARITE_MIN * 4.0f * r.a * r.b;
    float tanPente = 0.0f;
    if (deuxPans) {
        const float tanMin = ardoise ? TAN_PENTE_ARDOISE : TAN_PENTE_AUTRE;
        tanPente = std::clamp(hauteurToit / r.b, tanMin, TAN_PENTE_MAX);
    }

    /* Distance signée au faîtage, et hauteur du toit en un point de l'emprise. */
    const auto ecart = [&](float x, float z) { return (x - r.cx) * r.vx + (z - r.cz) * r.vz; };
    const auto hautEn = [&](float x, float z) {
        return top + (r.b - std::fabs(ecart(x, z))) * tanPente;
    };

    /* Deux faces opposées ont des normales opposées : le critère est pris en
       valeur absolue, sinon elles recevraient des habillages différents
       (fenêtres d'un côté, mur nu en face).

       Le résultat voyage jusqu'au fragment par le SIGNE de l'UV horizontal :
       la structure Vertex est partagée avec le terrain et son million de
       sommets, lui ajouter un attribut coûterait 4 Mo par carte. Voir
       building.frag. */
    const vec3 ref = normaleDeReference(px, pz);

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j  = (i + 1) % n;
        const float       ex = px[j] - px[i];
        const float       ez = pz[j] - pz[i];

        /* Normale horizontale perpendiculaire au côté, vers l'extérieur : à
           droite du côté si l'emprise tourne dans le sens direct, à gauche
           sinon. Le sens de parcours vaut pour toute emprise, concave ou
           trouée (le mur d'une cour regarde la cour). */
        const float sens   = signee >= 0.0f ? 1.0f : -1.0f;
        const vec3  normal = glm::normalize(vec3{sens * ez, 0.0f, -sens * ex});

        const float sideLen  = std::sqrt(ex * ex + ez * ez);
        const float signe    = std::fabs(normal.x * ref.x + normal.z * ref.z) >= COS_45 ? 1.0f : -1.0f;

        /* Le mur monte jusque sous le toit. Un côté qui passe sous le faîtage
           (un pignon) est coupé là, pour que son sommet suive les deux pans. */
        const auto pan = [&](float t0, float t1) {
            const float x0 = px[i] + t0 * ex, z0 = pz[i] + t0 * ez;
            const float x1 = px[i] + t1 * ex, z1 = pz[i] + t1 * ez;
            const float h0 = hautEn(x0, z0), h1 = hautEn(x1, z1);
            pushWallQuad(verts, idx, vec3{x0, base, z0}, vec3{x1, base, z1}, vec3{x1, h1, z1},
                         vec3{x0, h0, z0}, normal, mur, signe * t0 * sideLen / FACADE_TILE_W_M,
                         signe * t1 * sideLen / FACADE_TILE_W_M, vBase + (h0 - base) * vParM,
                         vBase + (h1 - base) * vParM, vBase);
        };
        const float si = ecart(px[i], pz[i]);
        const float sj = ecart(px[j], pz[j]);
        if (tanPente > 0.0f && si * sj < 0.0f) {
            const float t = si / (si - sj);
            pan(0.0f, t);
            pan(t, 1.0f);
        } else {
            pan(0.0f, 1.0f);
        }
    }

    if (!deuxPans) {
        const vec3                     up{0.0f, 1.0f, 0.0f};
        const std::vector<std::size_t> tris = trianguler(px, pz);
        for (std::size_t k = 0; k + 2 < tris.size(); k += 3) {
            const std::size_t a = tris[k], b = tris[k + 1], c = tris[k + 2];
            pushTriangle(verts, idx, vec3{px[a], top, pz[a]}, vec3{px[b], top, pz[b]},
                         vec3{px[c], top, pz[c]}, up, toit);
        }
        if (hauteur < HAUTEUR_IMMEUBLE_M) {
            return top;
        }
        /* Terrasse d'immeuble : l'édicule au centre du plus grand triangle (un
           point sûrement dans l'emprise, même en L), les climatiseurs au centre
           des suivants. Les triangles trop petits pour la boîte sont passés. */
        std::vector<std::pair<float, std::size_t>> ordre; /* (-surface, triangle) */
        for (std::size_t k = 0; k + 2 < tris.size(); k += 3) {
            const std::size_t a = tris[k], b = tris[k + 1], c = tris[k + 2];
            const float s = std::fabs((px[b] - px[a]) * (pz[c] - pz[a]) -
                                      (px[c] - px[a]) * (pz[b] - pz[a]));
            ordre.emplace_back(-s, k);
        }
        std::sort(ordre.begin(), ordre.end());
        float sommet = top;
        int   poses  = 0;
        for (const auto& [moins, k] : ordre) {
            const std::size_t a = tris[k], b = tris[k + 1], c = tris[k + 2];
            const float cx = (px[a] + px[b] + px[c]) / 3.0f;
            const float cz = (pz[a] + pz[b] + pz[c]) / 3.0f;
            const bool  edicule = poses == 0;
            const float demiL = edicule ? EDICULE_DEMI_L_M : CLIM_DEMI_M;
            const float demiW = edicule ? EDICULE_DEMI_W_M : CLIM_DEMI_M;
            const float haut  = top + (edicule ? EDICULE_HAUT_M : CLIM_HAUT_M);
            if (poserBoite(verts, idx, px, pz, cx, cz, r.ux, r.uz, demiL, demiW, top, haut,
                           facade, mur, toit)) {
                sommet = std::max(sommet, haut);
                if (++poses > CLIM_MAX) {
                    break;
                }
            }
        }
        return sommet;
    }

    /* Les deux pans, débord compris : le faîtage sur le grand axe, les égouts
       DEBORD_M au-delà des murs, donc un peu plus bas que la gouttière. */
    const float faitage = top + r.b * tanPente;
    const float egout   = top - DEBORD_M * tanPente;
    const float a       = r.a + DEBORD_M;
    const float b       = r.b + DEBORD_M;
    /* UV des pans : u le long du faîtage, v le long de la pente, en mètres
       réels, pour que les rangs de tuiles suivent la pente. */
    const float uA = a / COUVERTURE_TILE_M;
    const float vB = b * std::sqrt(1.0f + tanPente * tanPente) / COUVERTURE_TILE_M;
    for (const float cote : {1.0f, -1.0f}) {
        const float vx = cote * r.vx, vz = cote * r.vz;
        const vec3  normal = glm::normalize(vec3{vx * tanPente, 1.0f, vz * tanPente});
        pushQuad(verts, idx, vec3{r.cx - a * r.ux, faitage, r.cz - a * r.uz},
                 vec3{r.cx + a * r.ux, faitage, r.cz + a * r.uz},
                 vec3{r.cx + a * r.ux + b * vx, egout, r.cz + a * r.uz + b * vz},
                 vec3{r.cx - a * r.ux + b * vx, egout, r.cz - a * r.uz + b * vz}, normal, toit,
                 vec2{-uA, 0.0f}, vec2{uA, 0.0f}, vec2{uA, vB}, vec2{-uA, vB});
    }
    return faitage;
}

/* Quad de chaussée : même géométrie qu'un pushQuad, mais l'UV porte le drapage
   de l'orthophoto au lieu d'être nul. Dessiné avec le shader du terrain. */
void pushQuadChaussee(std::vector<Vertex>&       verts,
                      std::vector<unsigned int>& idx,
                      const vec3&                a,
                      const vec3&                b,
                      const vec3&                c,
                      const vec3&                d,
                      const vec3&                normal,
                      const CalageOrtho&         ortho) {
    const vec3 blanc{1.0f, 1.0f, 1.0f}; /* le shader du terrain ignore la couleur */
    const auto base = static_cast<unsigned int>(verts.size());
    verts.push_back(Vertex{a, normal, blanc, ortho.uv(a.x, a.z)});
    verts.push_back(Vertex{b, normal, blanc, ortho.uv(b.x, b.z)});
    verts.push_back(Vertex{c, normal, blanc, ortho.uv(c.x, c.z)});
    verts.push_back(Vertex{d, normal, blanc, ortho.uv(d.x, d.z)});
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
}

void extruderTablier(std::vector<Vertex>&       vertsBeton,
                     std::vector<unsigned int>& idxBeton,
                     std::vector<Vertex>&       vertsChaussee,
                     std::vector<unsigned int>& idxChaussee,
                     const std::vector<float>&  px,
                     const std::vector<float>&  py,
                     const std::vector<float>&  pz,
                     float                      demiGauche,
                     float                      demiDroite,
                     const vec3&                couleur,
                     const CalageOrtho&         ortho) {
    const std::size_t n = px.size();
    if (n < 2) {
        return;
    }

    /* Les flancs sont verticaux dans la réalité, mais le shader trie mur et
       toit sur la seule normale (voir building.frag) : une normale horizontale
       leur collerait la texture de façade, fenêtres comprises. On les incline
       donc vers le haut, ce qui donne au passage un chanfrein plus proche d'un
       bord de tablier que d'une arête vive. */
    constexpr float FLANC_HORIZ = 0.8f;
    constexpr float FLANC_VERT  = 0.6f;
    const vec3      haut{0.0f, 1.0f, 0.0f};
    const vec3      bas{0.0f, -1.0f, 0.0f};

    for (std::size_t i = 0; i + 1 < n; ++i) {
        const std::size_t j = i + 1;
        const vec3        pi = perpendiculaireAxe(px, pz, i);
        const vec3        pj = perpendiculaireAxe(px, pz, j);

        const float basI = py[i] - EPAISSEUR_TABLIER_M;
        const float basJ = py[j] - EPAISSEUR_TABLIER_M;

        /* La perpendiculaire pointe à GAUCHE du sens de parcours. */
        const vec3 hgI{px[i] + pi.x * demiGauche, py[i], pz[i] + pi.z * demiGauche};
        const vec3 hdI{px[i] - pi.x * demiDroite, py[i], pz[i] - pi.z * demiDroite};
        const vec3 hgJ{px[j] + pj.x * demiGauche, py[j], pz[j] + pj.z * demiGauche};
        const vec3 hdJ{px[j] - pj.x * demiDroite, py[j], pz[j] - pj.z * demiDroite};
        const vec3 bgI{hgI.x, basI, hgI.z};
        const vec3 bdI{hdI.x, basI, hdI.z};
        const vec3 bgJ{hgJ.x, basJ, hgJ.z};
        const vec3 bdJ{hdJ.x, basJ, hdJ.z};

        pushQuadChaussee(vertsChaussee, idxChaussee, hgI, hdI, hdJ, hgJ, haut, ortho);
        pushQuad(vertsBeton, idxBeton, bgI, bgJ, bdJ, bdI, bas, couleur * 0.75f);

        const vec3 nG = glm::normalize(vec3{pi.x * FLANC_HORIZ, FLANC_VERT, pi.z * FLANC_HORIZ});
        const vec3 nD = glm::normalize(vec3{-pi.x * FLANC_HORIZ, FLANC_VERT, -pi.z * FLANC_HORIZ});
        pushQuad(vertsBeton, idxBeton, hgI, hgJ, bgJ, bgI, nG, couleur);
        pushQuad(vertsBeton, idxBeton, hdI, bdI, bdJ, hdJ, nD, couleur);

        /* Capots aux deux bouts : la BD TOPO coupe le réseau à chaque
           intersection, un pont est donc rendu en plusieurs tronçons qui
           s'arrêtent net. Sans capot on voit à travers la tranche. */
        if (i == 0 || j + 1 == n) {
            /* Direction du segment, NORMALISÉE avant pondération : sans cela le
               poids vertical serait noyé par la longueur du segment et la
               normale repasserait à l'horizontale, donc en mur de façade. */
            const float ex   = px[j] - px[i];
            const float ez   = pz[j] - pz[i];
            const float len2 = ex * ex + ez * ez;
            const float inv  = (len2 > 1e-12f) ? 1.0f / std::sqrt(len2) : 0.0f;
            const vec3  axe{ex * inv * FLANC_HORIZ, FLANC_VERT, ez * inv * FLANC_HORIZ};
            if (i == 0) {
                pushQuad(vertsBeton, idxBeton, hgI, bgI, bdI, hdI,
                         glm::normalize(vec3{-axe.x, axe.y, -axe.z}), couleur);
            }
            if (j + 1 == n) {
                pushQuad(vertsBeton, idxBeton, hgJ, hdJ, bdJ, bgJ, glm::normalize(axe), couleur);
            }
        }
    }
}

std::vector<SegmentTablier> segmentsProches(const std::vector<SegmentTablier>& segments,
                                            float x, float z, float rayon) {
    /* Distance du point au segment, bord du tablier compris. Une passe sur
       tous les segments à chaque image : quelques centaines sur une carte. */
    std::vector<std::pair<float, const SegmentTablier*>> proches;
    const vec2 p{x, z};
    for (const SegmentTablier& s : segments) {
        const vec2  ab = s.b - s.a;
        const float l2 = glm::dot(ab, ab);
        const float t  = (l2 > 0.0f) ? std::clamp(glm::dot(p - s.a, ab) / l2, 0.0f, 1.0f) : 0.0f;
        const float d  = glm::length(p - (s.a + t * ab)) - std::max(s.gauche, s.droite);
        if (d < rayon) {
            proches.emplace_back(d, &s);
        }
    }
    std::sort(proches.begin(), proches.end(),
              [](const auto& u, const auto& v) { return u.first < v.first; });

    std::vector<SegmentTablier> retenus;
    for (std::size_t k = 0; k < proches.size() && k < TABLIERS_EFFACES_MAX; ++k) {
        retenus.push_back(*proches[k].second);
    }
    return retenus;
}

} /* namespace artouste::render */
