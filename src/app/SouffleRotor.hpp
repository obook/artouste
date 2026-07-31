/*
 * SouffleRotor.hpp
 * Souffle du rotor au ras du sol : la poussière soulevée en anneau sous
 * l'appareil quand il approche du terrain, qui s'écarte vers l'extérieur puis
 * remonte en fin de course (recirculation en tore). C'est l'effet que
 * FlightGear obtient avec un système de particules déclaré dans le modèle de
 * l'appareil, conditionné à la hauteur sol et au régime rotor.
 *
 * Cette classe ne tient que la simulation des bouffées : émission, intégration,
 * durée de vie. Aucune ressource graphique, donc elle se teste sans contexte
 * OpenGL, comme app::ProjectileSystem. Le dessin est dans render::SouffleFx
 * (billboards instanciés) et Application::drawSouffle.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <cstddef>
#include <functional>
#include <vector>

namespace artouste::app {

class SouffleRotor {
public:
    /* Une bouffée telle que le rendu la reçoit : centre monde, diamètre et
       opacité du billboard, plus une graine et une rotation qui distinguent son
       grain à l'écran (deux bouffées voisines ne doivent pas se superposer à
       l'identique). */
    struct Bouffee {
        vec3 centre{0.0f};
        float diametre = 1.0f;
        float opacite = 0.0f;
        float graine = 0.0f;
        float rotation = 0.0f;
        /* Hauteur au-dessus du terrain (m). Le rendu s'en sert pour effacer une
           bouffée trop grosse pour la place qu'elle a sous elle : un billboard
           vertical qui traverse le relief est coupé net par le test de
           profondeur, et toutes celles de la même hauteur étant coupées sur la
           même ligne, un trait barrerait le nuage en travers. */
        float hauteurSol = 0.0f;
    };

    /* rayonRotor : rayon du rotor principal (m). Il fixe l'anneau d'émission au
       sol et, par ricochet, la hauteur au-delà de laquelle le souffle ne soulève
       plus rien (voir plafondM). capacite : nombre maximal de bouffées vivantes,
       le seul plafond de coût de l'effet. */
    explicit SouffleRotor(float rayonRotor = 5.0f, std::size_t capacite = 420);

    /* Hauteur sol (m) au-delà de laquelle plus rien n'est soulevé. Exprimée en
       rayons rotor : c'est la portée du flux descendant, pas une distance
       absolue. */
    [[nodiscard]] float plafondM() const noexcept { return m_rayonRotor * PLAFOND_EN_RAYONS; }

    /* Intensité du soulèvement, dans [0, 1] : ce qui décide du débit de bouffées
       ET de leur opacité. Maximale au contact du sol, rotor au régime et plein
       pas ; elle décroît régulièrement avec la hauteur jusqu'à s'annuler au
       plafond, et nulle rotor arrêté. Fonction pure, sans effet de bord : c'est
       elle que vérifient les essais. */
    [[nodiscard]] float
    intensite(float hauteurSol, float rotorFraction, float collectif) const noexcept;

    /* Avance la simulation d'un pas. centreRotor est la position MONDE de l'axe
       du mât (et non celle du centre de l'appareil) : c'est de là que tombe le
       flux. hauteurSol rend l'altitude du terrain en un point (x, z) ; elle est
       appelée à la naissance de chaque bouffée et à chaque pas pour les garder
       posées sur le relief. Un pas nul (simulation figée) ne change rien. */
    void update(float dt,
                const vec3& centreRotor,
                float rotorFraction,
                float collectif,
                const std::function<float(float, float)>& hauteurSol);

    /* Bouffées vivantes, reconstruites à chaque update. */
    [[nodiscard]] const std::vector<Bouffee>& bouffees() const noexcept { return m_bouffees; }

    /* Efface tout (changement de carte, retour au menu) : sans cela le nuage de
       l'ancienne carte réapparaîtrait aux coordonnées de la nouvelle. */
    void vider() noexcept;

private:
    /* État interne d'une bouffée. Les grandeurs figées à la naissance (vie,
       diamètre, opacité) portent un 0 : le rendu, lui, voit leur valeur du
       moment. */
    struct Particule {
        vec3 position{0.0f};
        vec3 vitesse{0.0f};
        float age = 0.0f;
        float vie = 1.0f;
        float diametre0 = 1.0f;
        float opacite0 = 0.0f;
        float graine = 0.0f;
        float rotation = 0.0f;
        float vitesseRotation = 0.0f;
        float hauteurSol = 0.0f; /* au-dessus du terrain, relevée à chaque pas */
    };

    /* Fait naître une bouffée sur l'anneau au sol, sous le disque rotor. */
    void emettre(const vec3& centreRotor,
                 float force,
                 const std::function<float(float, float)>& hauteurSol);

    /* Tirage uniforme dans [0, 1). Générateur maison, reproductible d'une
       exécution à l'autre : les essais en dépendent. */
    [[nodiscard]] float alea() noexcept;

    /* Portée du flux descendant, en rayons rotor : un peu moins de trois, ce qui
       donne 14 m pour l'Alouette II. Au-delà, le souffle s'est dispersé avant
       d'atteindre le sol. */
    static constexpr float PLAFOND_EN_RAYONS = 2.8f;

    float m_rayonRotor = 5.0f;
    std::size_t m_capacite = 0;
    std::vector<Particule> m_particules;
    std::vector<Bouffee> m_bouffees;
    /* Fraction de bouffée en attente d'émission, reportée d'une image à l'autre :
       sans ce report, un pas court n'émettrait jamais rien et le débit dépendrait
       de la cadence d'affichage. */
    float m_reste = 0.0f;
    unsigned int m_etatAlea = 0x9e3779b9u; /* état du tirage pseudo-aléatoire */
};

} /* namespace artouste::app */
