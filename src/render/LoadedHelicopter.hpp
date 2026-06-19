// Helicoptère assemblé à partir du modèle FlightGear (.ac) : fuselage texturé +
// rotors (moyeu + pales chargés séparément), animés à régime fixe.
//
// Le modèle FlightGear est en repère X = arrière, Y = latéral, Z = haut. Assimp
// le convertit en Y-up (X longitudinal, Y haut, Z latéral) ; on applique encore
// une rotation de 180° (le nez FlightGear est en -X, notre avant est +X) et un
// décalage vertical pour poser les patins au sol. Échelle déjà en mètres.

#pragma once

#include "render/Model.hpp"
#include "util/Math.hpp"

#include <filesystem>

namespace artouste::render {

class Shader;

class LoadedHelicopter {
public:
    explicit LoadedHelicopter(const std::filesystem::path& modelsDir);

    // Vrai si au moins le fuselage a été chargé (sinon : retomber sur le
    // placeholder procédural).
    [[nodiscard]] bool loaded() const noexcept { return !m_fuselage.empty(); }

    // base : transformation monde de l'appareil ; timeSeconds anime les rotors.
    void draw(Shader& shader, const mat4& base, float timeSeconds) const;

private:
    Model m_fuselage;
    Model m_interior;
    Model m_mainHub;
    Model m_mainBlade;
    Model m_tailHub;
    Model m_tailBlade;
};

}  // namespace artouste::render
