#include "render/HelicopterModel.hpp"

#include "render/Primitives.hpp"
#include "render/Shader.hpp"

namespace artouste::render {

namespace {

// Régimes d'animation purement visuels (rad/s). Le régime nominal réel de
// l'Alouette II (~354 tr/min, soit ~37 rad/s) provoque un effet stroboscopique
// à 60 Hz ; on ralentit donc l'affichage. Les valeurs physiques exactes
// serviront au modèle de vol (M3), pas au rendu.
constexpr float MAIN_ROTOR_SPEED = 18.0f;
constexpr float TAIL_ROTOR_SPEED = 55.0f;

Mesh toMesh(const primitives::MeshData& data) {
    return Mesh(data.vertices, data.indices);
}

}  // namespace

HelicopterModel::HelicopterModel() {
    using primitives::bladeSet;
    using primitives::box;

    const vec3 cabinColor{0.22f, 0.34f, 0.48f};
    const vec3 metalColor{0.55f, 0.57f, 0.60f};
    const vec3 darkColor{0.16f, 0.16f, 0.18f};
    const vec3 rotorColor{0.12f, 0.12f, 0.14f};

    m_cabin     = toMesh(box({1.45f, 1.10f, 1.05f}, cabinColor));
    m_boom      = toMesh(box({3.00f, 0.16f, 0.16f}, metalColor));
    m_mast      = toMesh(box({0.12f, 0.50f, 0.12f}, darkColor));
    m_fin       = toMesh(box({0.45f, 0.55f, 0.05f}, metalColor));
    m_skidLeft  = toMesh(box({1.70f, 0.07f, 0.07f}, darkColor));
    m_skidRight = toMesh(box({1.70f, 0.07f, 0.07f}, darkColor));
    m_mainRotor = toMesh(bladeSet(3, 5.00f, 0.35f, 0.06f, rotorColor));
    m_tailRotor = toMesh(bladeSet(3, 0.85f, 0.16f, 0.04f, rotorColor));
}

void HelicopterModel::draw(Shader& shader, const mat4& base, float timeSeconds) const {
    const auto drawPart = [&](const Mesh& mesh, const mat4& local) {
        shader.setMat4("u_model", base * local);
        mesh.draw();
    };

    drawPart(m_cabin, glm::translate(mat4(1.0f), vec3{0.40f, 1.35f, 0.00f}));
    drawPart(m_boom, glm::translate(mat4(1.0f), vec3{-3.20f, 1.75f, 0.00f}));
    drawPart(m_mast, glm::translate(mat4(1.0f), vec3{0.20f, 2.55f, 0.00f}));
    drawPart(m_fin, glm::translate(mat4(1.0f), vec3{-6.00f, 2.05f, 0.00f}));
    drawPart(m_skidLeft, glm::translate(mat4(1.0f), vec3{0.20f, 0.10f, 0.90f}));
    drawPart(m_skidRight, glm::translate(mat4(1.0f), vec3{0.20f, 0.10f, -0.90f}));

    // Rotor principal : rotation autour de l'axe vertical (Y).
    const float mainAngle = timeSeconds * MAIN_ROTOR_SPEED;
    const mat4  mainXform = glm::translate(mat4(1.0f), vec3{0.20f, 3.05f, 0.00f}) *
                           glm::rotate(mat4(1.0f), mainAngle, vec3{0.0f, 1.0f, 0.0f});
    drawPart(m_mainRotor, mainXform);

    // Rotor de queue : disque basculé à la verticale, rotation autour de l'axe
    // latéral. La rotation -90 degrés autour de X redresse le disque ; le spin
    // s'applique avant, autour du Y local (donc autour de Z après bascule).
    const float tailAngle = timeSeconds * TAIL_ROTOR_SPEED;
    const mat4  tailXform = glm::translate(mat4(1.0f), vec3{-6.15f, 1.90f, 0.22f}) *
                           glm::rotate(mat4(1.0f), -HALF_PI, vec3{1.0f, 0.0f, 0.0f}) *
                           glm::rotate(mat4(1.0f), tailAngle, vec3{0.0f, 1.0f, 0.0f});
    drawPart(m_tailRotor, tailXform);
}

}  // namespace artouste::render
