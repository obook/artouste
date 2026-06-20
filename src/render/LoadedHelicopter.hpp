/*
 * LoadedHelicopter.hpp
 * Hélicoptère assemblé à partir des fichiers du modèle FlightGear (.ac) :
 * fuselage texturé, intérieur, planche de bord et rotors (moyeu et pales
 * chargés séparément, puis animés à régime fixe).
 *
 * Les fichiers FlightGear utilisent un repère différent du nôtre. Au chargement
 * et au dessin, on applique donc des corrections (rotation de 180 degrés et
 * décalage vertical pour poser les patins au sol) afin d'orienter et de placer
 * l'appareil correctement dans la scène.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Model.hpp"
#include "util/Math.hpp"

#include <filesystem>
#include <vector>

namespace artouste::render {

class Shader;

class LoadedHelicopter {
public:
    explicit LoadedHelicopter(const std::filesystem::path& modelsDir);

    /* Vrai si le fuselage a bien été chargé. Sinon, l'appelant se rabat sur
       l'hélicoptère de remplacement construit par le code. */
    [[nodiscard]] bool loaded() const noexcept { return !m_fuselage.empty(); }

    /* Dessine l'appareil complet. 'base' place et oriente l'hélicoptère dans le
       monde ; 'rotorAngle' est l'angle du rotor principal (rad), géré par
       l'application (rotation au régime du rotor, puis parking à l'arrêt). */
    void draw(Shader& shader, const mat4& base, float rotorAngle) const;

    /* Centre du disque rotor dans le monde (pour dessiner un jour le disque flou
       translucide à haut régime, voir le code mis en commentaire). 'base' est la
       pose de l'appareil. */
    [[nodiscard]] vec3 mainHubWorld(const mat4& base) const;

    /* Rayon du rotor principal, en mètres (longueur d'une pale). */
    static constexpr float MAIN_ROTOR_RADIUS = 5.0f;

private:
    /* Un cadran de la planche de bord avec sa position relative au panneau. */
    struct Gauge {
        Model model;
        vec3  offset;
    };

    Model              m_fuselage;
    Model              m_interior;
    Model              m_panel;
    std::vector<Gauge> m_gauges;
    Model              m_mainHub;
    Model              m_mainBlade;
    Model              m_tailHub;
    Model              m_tailBlade;
};

}  /* namespace artouste::render */
