#include "render/LoadedHelicopter.hpp"

#include <glad/glad.h>

#include "render/ModelLoader.hpp"
#include "render/Shader.hpp"

#include <cstdio>
#include <utility>

namespace artouste::render {

namespace {

// --- Correction de repère FlightGear -> moteur -------------------------------
constexpr float Y_OFFSET = 1.951f;  // remonte les patins au niveau du sol

// --- Placements des rotors (repère Assimp, avant correction) -----------------
// Issus des fichiers d'assemblage FlightGear (all-*rotor.xml). Coordonnées
// FlightGear (x, y, z) -> Assimp (x, z, y).
const vec3  MAIN_HUB{-2.142f, 0.177f, 0.000f};
constexpr float MAIN_BLADE_RISE = 0.826f;
constexpr int   MAIN_BLADES     = 3;

const vec3  TAIL_HUB{3.963f, 0.174f, 0.226f};
constexpr float TAIL_BLADE_RISE = 0.031f;
constexpr int   TAIL_BLADES     = 2;

// Régimes d'animation purement visuels (rad/s), ralentis pour éviter l'effet
// stroboscopique. Le rotor de queue tourne environ 5x plus vite (ratio réel).
constexpr float MAIN_SPIN = 15.0f;
constexpr float TAIL_SPIN = 75.0f;

// Conversion d'un décalage FlightGear (x avant/arrière, y latéral, z vertical)
// vers le repère Assimp (x, y vertical, z latéral).
vec3 fgToAssimp(const vec3& fg) {
    return vec3{fg.x, fg.z, fg.y};
}

// Planche de bord : offset d'assemblage (all-panels.xml).
const vec3 PANEL_OFFSET = fgToAssimp(vec3{-4.136f, 0.0f, -0.344f});

// Cadrans de la planche : fichier .ac et offset FlightGear relatif au panneau
// (panel.xml). Deux rangées de quatre. Aiguilles figées (statiques).
struct GaugeDef {
    const char* file;
    vec3        fgOffset;
};
const GaugeDef GAUGES[] = {
    {"Interior/Panel/Instruments/alt/alt.ac", vec3{-0.222f, -0.181f, 0.112f}},
    {"Interior/Panel/Instruments/vsi/vsi.ac", vec3{-0.208f, -0.060f, 0.112f}},
    {"Interior/Panel/Instruments/ai/ai.ac", vec3{-0.231f, 0.060f, 0.112f}},
    {"Interior/Panel/Instruments/asi/asi.ac", vec3{-0.207f, 0.181f, 0.112f}},
    {"Interior/Panel/Instruments/vor/vor.ac", vec3{-0.223f, -0.181f, -0.014f}},
    {"Interior/Panel/Instruments/hi/hi.ac", vec3{-0.223f, -0.060f, -0.014f}},
    {"Interior/Panel/Instruments/rmi/rmi.ac", vec3{-0.224f, 0.060f, -0.014f}},
    {"Interior/Panel/Instruments/torque/torque.ac", vec3{-0.223f, 0.181f, -0.014f}},
};

Model loadPart(const std::filesystem::path& path, const std::vector<std::string>& skip,
               const std::vector<std::string>& transparent = {}) {
    Model model = ModelLoader::load(path, skip, transparent);
    std::printf("[modèle] %-44s : %zu parties\n", path.filename().string().c_str(),
                model.partCount());
    return model;
}

}  // namespace

LoadedHelicopter::LoadedHelicopter(const std::filesystem::path& dir) {
    // On ignore : les plans semi-transparents (blur/disc) des rotors, les
    // doublons HDR des vitrages (rendu opaque), et la version flotteurs du
    // train (on garde le mode patins, comme FlightGear par défaut).
    const std::vector<std::string> skipBody{"hdr", "blur", "disc", "flotteur", "barre"};
    const std::vector<std::string> skipRotor{"blur", "disc"};
    // Vitrages rendus en translucide (verrière de cabine, vitres de portes).
    const std::vector<std::string> glass{"verriere", "vitre"};

    m_fuselage  = loadPart(dir / "alouette.ac", skipBody, glass);
    m_interior  = loadPart(dir / "Interior/interior.ac", skipBody, glass);

    // Planche de bord + cadrans (statiques).
    m_panel = loadPart(dir / "Interior/Panel/panel.ac", skipRotor);
    for (const GaugeDef& def : GAUGES) {
        Gauge gauge;
        gauge.model  = loadPart(dir / def.file, skipRotor);
        gauge.offset = PANEL_OFFSET + fgToAssimp(def.fgOffset);
        m_gauges.push_back(std::move(gauge));
    }

    m_mainHub   = loadPart(dir / "Externals/MainRotor/mainrotor.ac", skipRotor);
    m_mainBlade = loadPart(dir / "Externals/MainRotor/blade.ac", skipRotor);
    m_tailHub   = loadPart(dir / "Externals/TailRotor/tailrotor.ac", skipRotor);
    m_tailBlade = loadPart(dir / "Externals/TailRotor/blade.ac", skipRotor);
}

void LoadedHelicopter::draw(Shader& shader, const mat4& base, float timeSeconds) const {
    // Correction de repère commune : 180° autour de Y puis remontée verticale.
    const mat4 correction = glm::translate(mat4(1.0f), vec3{0.0f, Y_OFFSET, 0.0f}) *
                            glm::rotate(mat4(1.0f), PI, vec3{0.0f, 1.0f, 0.0f});
    const mat4 root = base * correction;

    const auto drawModel = [&](const Model& model, const mat4& transform, Pass pass) {
        shader.setMat4("u_model", transform);
        model.draw(shader, pass);
    };

    // Transformations des rotors (réutilisées par les deux passes).
    const float mainAngle = timeSeconds * MAIN_SPIN;
    const mat4  mainBase  = root * glm::translate(mat4(1.0f), MAIN_HUB) *
                           glm::rotate(mat4(1.0f), mainAngle, vec3{0.0f, 1.0f, 0.0f});
    // Rotor de queue : disque basculé à la verticale (-90° autour de X), puis
    // rotation autour de son axe (Y local), ce qui place le spin sur l'axe
    // latéral après bascule.
    const float tailAngle = timeSeconds * TAIL_SPIN;
    const mat4  tailBase  = root * glm::translate(mat4(1.0f), TAIL_HUB) *
                           glm::rotate(mat4(1.0f), -HALF_PI, vec3{1.0f, 0.0f, 0.0f}) *
                           glm::rotate(mat4(1.0f), tailAngle, vec3{0.0f, 1.0f, 0.0f});

    const auto drawRotors = [&](Pass pass) {
        drawModel(m_mainHub, mainBase, pass);
        for (int k = 0; k < MAIN_BLADES; ++k) {
            const float heading =
                static_cast<float>(k) * (TWO_PI / static_cast<float>(MAIN_BLADES));
            const mat4 bladeMat = mainBase *
                                  glm::rotate(mat4(1.0f), heading, vec3{0.0f, 1.0f, 0.0f}) *
                                  glm::translate(mat4(1.0f), vec3{0.0f, MAIN_BLADE_RISE, 0.0f});
            drawModel(m_mainBlade, bladeMat, pass);
        }
        drawModel(m_tailHub, tailBase, pass);
        for (int k = 0; k < TAIL_BLADES; ++k) {
            const float heading =
                static_cast<float>(k) * (TWO_PI / static_cast<float>(TAIL_BLADES));
            const mat4 bladeMat = tailBase *
                                  glm::rotate(mat4(1.0f), heading, vec3{0.0f, 1.0f, 0.0f}) *
                                  glm::translate(mat4(1.0f), vec3{0.0f, TAIL_BLADE_RISE, 0.0f});
            drawModel(m_tailBlade, bladeMat, pass);
        }
    };

    // Passe opaque : fuselage (hors vitrages) + intérieur + planche + rotors.
    drawModel(m_fuselage, root, Pass::Opaque);
    drawModel(m_interior, root, Pass::Opaque);
    drawModel(m_panel, root * glm::translate(mat4(1.0f), PANEL_OFFSET), Pass::Opaque);
    for (const Gauge& gauge : m_gauges) {
        drawModel(gauge.model, root * glm::translate(mat4(1.0f), gauge.offset), Pass::Opaque);
    }
    drawRotors(Pass::Opaque);

    // Passe transparente : vitrages, avec mélange alpha et profondeur en
    // lecture seule pour ne pas masquer ce qui est derrière.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    drawModel(m_fuselage, root, Pass::Transparent);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

}  // namespace artouste::render
