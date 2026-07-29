/*
 * SouffleRotor.cpp
 * Voir SouffleRotor.hpp.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#include "app/SouffleRotor.hpp"

#include <cmath>

namespace artouste::app {

namespace {

/* Débit de bouffées à pleine intensité (par seconde). Avec une vie moyenne de
   2,35 s, le nuage plafonne autour de 165 bouffées, sous la capacité que lui
   donne l'application (voir SOUFFLE_CAPACITY, ApplicationSceneShaders.cpp) :
   relever ce débit sans relever la capacité ne ferait rien de plus, l'émission
   s'arrête au plafond. Et ce n'est le débit qu'AU CONTACT du sol, pas en
   vol : l'intensité le divise très vite avec la hauteur (voir intensite). */
constexpr float DEBIT_MAX = 70.0f;

/* Durée de vie d'une bouffée (s) : assez longue pour qu'elle s'écarte loin et
   remonte, assez courte pour que le nuage suive l'appareil qui se déplace. */
constexpr float VIE_MIN = 1.8f;
constexpr float VIE_MAX = 2.9f;

/* Vitesse d'écartement au ras du sol (m/s). Le flux descendant frappe le sol et
   repart en nappe horizontale : c'est ce qui donne l'anneau qui s'ouvre. */
constexpr float VITESSE_RADIALE = 11.0f;

/* Part tangentielle de cette vitesse : la nappe tourne un peu dans le sens du
   rotor, ce qui donne la spirale plutôt qu'une onde parfaitement radiale. */
constexpr float PART_TANGENTIELLE = 0.22f;

/* Freinage horizontal (1/s) : l'air perd sa vitesse en s'éloignant du disque. */
constexpr float FROTTEMENT = 1.25f;

/* Accélération verticale en fin de vie (m/s^2), qui fait remonter la nappe :
   c'est la recirculation en tore. Sans elle l'effet reste une onde plate et ne
   ressemble à rien ; trop forte, le panache monte plus haut que la cabine et se
   lit comme de la fumée, ce qui se voit surtout quand le fond est sombre (le
   versant boisé d'Ossau) plutôt que sur un sol clair. */
constexpr float MONTEE = 2.2f;

/* Diamètre d'une bouffée à la naissance (m) et facteur de grossissement sur sa
   vie : la poussière se dilue en s'éloignant. */
constexpr float DIAMETRE_MIN = 1.6f;
constexpr float DIAMETRE_MAX = 2.8f;
constexpr float GROSSISSEMENT = 1.9f;

/* Opacité maximale d'une bouffée, avant le fondu du bord et le grain que lui
   applique le shader : à l'écran, une bouffée isolée est bien plus discrète que
   ce chiffre. Le nuage se construit par accumulation, mais trop bas cette valeur
   ne donne qu'un voile invisible sur un sol de la même teinte. */
constexpr float OPACITE_MAX = 0.38f;

/* Hauteur de naissance au-dessus du sol (m) et garde au sol conservée ensuite :
   la bouffée ne doit ni percer le terrain ni y coller au point de disparaître
   sous une pente. */
constexpr float HAUTEUR_NAISSANCE = 0.40f;
constexpr float GARDE_SOL = 0.30f;

} /* namespace */

SouffleRotor::SouffleRotor(float rayonRotor, std::size_t capacite)
    : m_rayonRotor(rayonRotor > 0.1f ? rayonRotor : 0.1f), m_capacite(capacite) {
    m_particules.reserve(m_capacite);
    m_bouffees.reserve(m_capacite);
}

float SouffleRotor::alea() noexcept {
    /* Générateur congruentiel linéaire minimal (constantes de Numerical Recipes).
       On garde les bits de poids fort, les bits de poids faible d'un tel
       générateur ayant une période très courte. */
    m_etatAlea = m_etatAlea * 1664525u + 1013904223u;
    return static_cast<float>((m_etatAlea >> 8) & 0xFFFFFFu) / 16777216.0f;
}

float SouffleRotor::intensite(float hauteurSol,
                              float rotorFraction,
                              float collectif) const noexcept {
    const float agl = hauteurSol > 0.0f ? hauteurSol : 0.0f;
    const float rotor = clamp(rotorFraction, 0.0f, 1.0f);
    /* Le souffle vient du rotor, et la poussée varie comme le carré du régime :
       à mi-régime, rotor encore en montée, presque rien ne décolle du sol. */
    const float souffle = rotor * rotor;
    /* Le pas collectif module sans jamais tout couper : rotor au régime,
       l'appareil brasse déjà de l'air manche en bas. */
    const float pas = 0.30f + 0.70f * clamp(collectif, 0.0f, 1.0f);
    /* Proximité du sol, décroissante DÈS le premier mètre et non par palier : le
       flux descendant s'étale et perd sa vitesse en descendant, donc il soulève
       de moins en moins haut, et plus rien au plafond. La décroissance est
       linéaire, et non en carré : un carré éteignait tout passé cinq mètres,
       alors qu'un stationnaire à dix mètres soulève encore un peu de poussière
       -- moins qu'au contact, mais on la voit. */
    const float proximite = 1.0f - clamp(agl / plafondM(), 0.0f, 1.0f);
    return clamp(souffle * pas * proximite, 0.0f, 1.0f);
}

void SouffleRotor::emettre(const vec3& centreRotor,
                           float force,
                           const std::function<float(float, float)>& hauteurSol) {
    const float angle = alea() * TWO_PI;
    /* Anneau d'émission plutôt que disque plein : le flux descendant frappe le
       sol sous le disque et s'écarte en nappe, l'axe du mât ne soulève presque
       rien. On tire donc entre un tiers du rayon et le bord. */
    const float rayon = m_rayonRotor * (0.35f + 0.65f * alea());
    const float cx = centreRotor.x + std::cos(angle) * rayon;
    const float cz = centreRotor.z + std::sin(angle) * rayon;

    const vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
    /* Sens tangentiel : celui du rotor de l'Alouette II, qui tourne dans le sens
       des aiguilles d'une montre vu de dessus (convention Sud-Aviation). */
    const vec3 tangent{std::sin(angle), 0.0f, -std::cos(angle)};
    const float vitesse = VITESSE_RADIALE * (0.65f + 0.5f * alea()) * (0.5f + 0.5f * force);

    Particule p;
    p.position = vec3{cx, hauteurSol(cx, cz) + HAUTEUR_NAISSANCE, cz};
    p.hauteurSol = HAUTEUR_NAISSANCE;
    p.vitesse =
        radial * vitesse + tangent * (vitesse * PART_TANGENTIELLE) + vec3{0.0f, 0.35f, 0.0f};
    p.vie = VIE_MIN + (VIE_MAX - VIE_MIN) * alea();
    p.diametre0 = DIAMETRE_MIN + (DIAMETRE_MAX - DIAMETRE_MIN) * alea();
    /* L'opacité suit largement l'intensité, et pas seulement le débit : sinon
       une poussière soulevée de loin serait aussi dense qu'au contact, elle
       serait juste plus rare. */
    p.opacite0 = OPACITE_MAX * (0.25f + 0.75f * force);
    p.graine = alea();
    p.rotation = alea() * TWO_PI;
    p.vitesseRotation = (alea() - 0.5f) * 1.2f;
    m_particules.push_back(p);
}

void SouffleRotor::update(float dt,
                          const vec3& centreRotor,
                          float rotorFraction,
                          float collectif,
                          const std::function<float(float, float)>& hauteurSol) {
    if (dt <= 0.0f || !hauteurSol) {
        return; /* simulation figée (pause) : le nuage reste tel quel */
    }

    /* Émission, au débit que commande l'intensité du moment. */
    const float solSousRotor = hauteurSol(centreRotor.x, centreRotor.z);
    const float force = intensite(centreRotor.y - solSousRotor, rotorFraction, collectif);
    m_reste += force * DEBIT_MAX * dt;
    int aNaitre = static_cast<int>(m_reste);
    m_reste -= static_cast<float>(aNaitre);
    while (aNaitre > 0 && m_particules.size() < m_capacite) {
        emettre(centreRotor, force, hauteurSol);
        --aNaitre;
    }

    /* Intégration. Le freinage est exponentiel plutôt que linéaire : il ne dépend
       alors pas du pas de temps, et une bouffée ne peut pas repartir en arrière
       sur une image longue. */
    const float freinage = std::exp(-FROTTEMENT * dt);
    for (std::size_t i = 0; i < m_particules.size();) {
        Particule& p = m_particules[i];
        p.age += dt;
        if (p.age >= p.vie) {
            /* Mort : on remplace par la dernière du tableau, l'ordre n'a pas
               d'importance (le rendu les traite toutes de la même façon). */
            p = m_particules.back();
            m_particules.pop_back();
            continue;
        }
        const float k = p.age / p.vie;
        p.vitesse.x *= freinage;
        p.vitesse.z *= freinage;
        /* Recirculation : la nappe qui s'écarte finit par remonter, d'autant plus
           qu'elle est vieille, ce qui referme le tore autour de l'appareil. */
        p.vitesse.y += MONTEE * k * dt;
        p.position += p.vitesse * dt;
        /* Le sol n'est pas plat : une bouffée qui court vers un talus s'y
           enfoncerait, et on la verrait disparaître dans la pente. */
        const float sol = hauteurSol(p.position.x, p.position.z);
        if (p.position.y < sol + GARDE_SOL) {
            p.position.y = sol + GARDE_SOL;
            p.vitesse.y = p.vitesse.y > 0.0f ? p.vitesse.y : 0.0f;
        }
        p.hauteurSol = p.position.y - sol;
        p.rotation += p.vitesseRotation * dt;
        ++i;
    }

    /* Vue pour le rendu : enveloppe d'opacité en cloche (montée franche,
       extinction douce) et grossissement continu. */
    m_bouffees.clear();
    for (const Particule& p : m_particules) {
        const float k = p.age / p.vie;
        Bouffee b;
        b.centre = p.position;
        b.diametre = p.diametre0 * (1.0f + (GROSSISSEMENT - 1.0f) * k);
        b.opacite = p.opacite0 * std::pow(std::sin(PI * k), 0.7f);
        b.graine = p.graine;
        b.rotation = p.rotation;
        b.hauteurSol = p.hauteurSol;
        m_bouffees.push_back(b);
    }
}

void SouffleRotor::vider() noexcept {
    m_particules.clear();
    m_bouffees.clear();
    m_reste = 0.0f;
}

} /* namespace artouste::app */
