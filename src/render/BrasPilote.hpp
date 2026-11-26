/*
 * BrasPilote.hpp
 * Pose du bras du pilote pour amener sa main sur une commande : cinématique
 * inverse à deux os, rigide. Le coude plie du même côté qu'au repos et ne se
 * tend jamais tout à fait ; le bras tourne autour de l'épaule et l'avant-bras
 * (main comprise) autour du coude. Si la cible est trop loin, l'épaule avance
 * un peu vers elle, puis les deux os s'allongent le long de leur axe.
 *
 * Auteur : O. Booklage
 * Date : novembre 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <cmath>

namespace artouste::render {

/* Le bras ne se tend jamais au-delà de cette part de sa longueur : le coude
   garde un pli. Pour aller plus loin, l'épaule avance d'abord vers la cible,
   au plus de EPAULE_AVANCE_MAX (le pilote se penche), puis les os
   s'allongent. */
inline constexpr float TENSION_MAX       = 0.95f;
inline constexpr float EPAULE_AVANCE_MAX = 0.06f;  /* m */

/* Articulations d'un bras au repos, dans le repère où l'on pose. */
struct OsBras {
    vec3 epaule;
    vec3 coude;
    vec3 main;
};

/* Transformations à appliquer au bras (épaule-coude) et à l'avant-bras
   (coude-main), depuis leur position de repos. */
struct PoseBras {
    mat4 bras;
    mat4 avantBras;
};

/* Rotation qui amène la direction 'de' sur la direction 'vers'. */
inline mat4 rotationEntre(const vec3& de, const vec3& vers) {
    const vec3  a     = glm::normalize(de);
    const vec3  b     = glm::normalize(vers);
    const vec3  axe   = glm::cross(a, b);
    const float sinus = glm::length(axe);
    if (sinus < 1e-6f) {
        return mat4(1.0f);  /* déjà alignés (les bras ne font jamais demi-tour) */
    }
    return glm::rotate(mat4(1.0f), std::atan2(sinus, glm::dot(a, b)), axe / sinus);
}

/* Allongement de facteur 'k' le long de la direction unitaire 'u'. */
inline mat4 allongement(const vec3& u, float k) {
    mat4 m(1.0f);
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            m[c][r] += (k - 1.0f) * u[c] * u[r];
        }
    }
    return m;
}

/* Os 'de' -> 'a' au repos, posé de 'origine' vers 'bout' : allongé de 'k',
   tourné, puis amené sur 'origine'. */
inline mat4 poserOs(const vec3& de, const vec3& a, const vec3& origine, const vec3& bout,
                    float k) {
    return glm::translate(mat4(1.0f), origine) * rotationEntre(a - de, bout - origine) *
           allongement(glm::normalize(a - de), k) * glm::translate(mat4(1.0f), -de);
}

/* Symétrie de l'avant-bras par le plan vertical qui le contient : coude et
   main restent, le pouce passe de l'autre côté. Sert quand la main du
   mannequin, tournée vers sa commande, se présente comme une main de
   l'autre côté. */
inline mat4 symetrieAvantBras(const OsBras& os) {
    const vec3 axe  = glm::normalize(os.main - os.coude);
    const vec3 cote = glm::normalize(glm::cross(axe, vec3{0.0f, 1.0f, 0.0f}));
    mat4       m(1.0f);
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            m[c][r] -= 2.0f * cote[c] * cote[r];
        }
    }
    return glm::translate(mat4(1.0f), os.coude) * m * glm::translate(mat4(1.0f), -os.coude);
}

/* Rotation de l'avant-bras (main comprise) sur son axe, de 'angleDeg'
   degrés dans le sens inverse des aiguilles d'une montre vu depuis le coude :
   tourne la main pour qu'elle se présente à sa commande. Coude et centre de
   la main restent en place. */
inline mat4 rotationAvantBras(const OsBras& os, float angleDeg) {
    const vec3 axe = glm::normalize(os.main - os.coude);
    return glm::translate(mat4(1.0f), os.coude) *
           glm::rotate(mat4(1.0f), glm::radians(-angleDeg), axe) *
           glm::translate(mat4(1.0f), -os.coude);
}

/* Pose le membre 'os' pour que son bout ('main') arrive sur 'cible'. Les
   jambes, presque tendues au repos et dont la hanche ne bouge pas, passent
   leur propre tension maximale et une avancée nulle. */
inline PoseBras poseBras(const OsBras& os, const vec3& cible, float tensionMax = TENSION_MAX,
                         float avanceMax = EPAULE_AVANCE_MAX) {
    const float l1 = glm::length(os.coude - os.epaule);
    const float l2 = glm::length(os.main - os.coude);
    const vec3  d  = cible - os.epaule;
    const float dl = glm::length(d);
    const vec3  u  = d / dl;

    /* Trop loin pour un bras encore plié : l'épaule avance d'abord, puis
       les os s'allongent juste assez. */
    const float portee = (l1 + l2) * tensionMax;
    const float avance = glm::clamp(dl - portee, 0.0f, avanceMax);
    const vec3  epaule = os.epaule + u * avance;
    const float reste  = dl - avance;
    const float k      = (reste > portee) ? reste / portee : 1.0f;
    const float a1     = l1 * k;
    const float a2     = l2 * k;

    /* Sens du pli : la part du coude de repos qui s'écarte de la ligne
       épaule-main de repos, ramenée perpendiculaire à la nouvelle ligne. */
    const vec3 ligne0 = glm::normalize(os.main - os.epaule);
    vec3       pli    = (os.coude - os.epaule) - ligne0 * glm::dot(os.coude - os.epaule, ligne0);
    pli               = pli - u * glm::dot(pli, u);
    pli = glm::length(pli) > 1e-6f ? glm::normalize(pli) : vec3{0.0f, -1.0f, 0.0f};

    /* Coude : triangle épaule-coude-main de côtés a1, a2, reste. */
    const float le    = (a1 * a1 - a2 * a2 + reste * reste) / (2.0f * reste);
    const float h     = std::sqrt(glm::max(0.0f, a1 * a1 - le * le));
    const vec3  coude = epaule + u * le + pli * h;

    return PoseBras{poserOs(os.epaule, os.coude, epaule, coude, k),
                    poserOs(os.coude, os.main, coude, cible, k)};
}

}  /* namespace artouste::render */
