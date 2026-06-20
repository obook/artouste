/*
 * LoadedHelicopter.cpp
 * Implémentation de l'hélicoptère chargé : assemblage des différentes pièces
 * (fuselage, intérieur, planche de bord, rotors), placement de chacune et
 * animation des rotors au fil du temps.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/LoadedHelicopter.hpp"

#include <glad/glad.h>

#include "render/ModelLoader.hpp"
#include "render/Shader.hpp"

#include <cstdio>
#include <utility>

namespace artouste::render {

namespace {

/* Correction de repère FlightGear vers notre moteur. */
constexpr float Y_OFFSET = 1.951f;  /* remonte les patins au niveau du sol */

/* Position des rotors (dans le repère du modèle, avant correction). Ces valeurs
   proviennent des fichiers d'assemblage de FlightGear. Une coordonnée
   FlightGear (x, y, z) correspond chez nous à (x, z, y). */
const vec3  MAIN_HUB{-2.142f, 0.177f, 0.000f};
constexpr float MAIN_BLADE_RISE = 0.826f;
constexpr int   MAIN_BLADES     = 3;

const vec3  TAIL_HUB{3.963f, 0.174f, 0.226f};
constexpr float TAIL_BLADE_RISE = 0.031f;
constexpr int   TAIL_BLADES     = 2;

/* Vitesses de rotation purement décoratives (en radians par seconde). Elles sont
   volontairement ralenties pour éviter l'effet stroboscopique. Le rotor de queue
   tourne environ 5 fois plus vite que le rotor principal, comme dans la réalité. */
constexpr float MAIN_SPIN = 15.0f;
constexpr float TAIL_SPIN = 75.0f;

/* Convertit une position exprimée dans le repère FlightGear (x avant/arrière,
   y latéral, z vertical) vers le repère du modèle (x, y vertical, z latéral). */
vec3 fgToAssimp(const vec3& fg) {
    return vec3{fg.x, fg.z, fg.y};
}

/* Position de la planche de bord (issue des fichiers d'assemblage FlightGear). */
const vec3 PANEL_OFFSET = fgToAssimp(vec3{-4.136f, 0.0f, -0.344f});

/* Description des cadrans de la planche de bord : le fichier .ac de chacun et sa
   position relative au panneau (coordonnées FlightGear). Ils forment deux rangées
   de quatre. Leurs aiguilles sont figées (purement décoratives). */
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

/* Charge une pièce du modèle et affiche un petit compte-rendu (nom du fichier et
   nombre de parties obtenues), pratique pour vérifier le chargement. */
Model loadPart(const std::filesystem::path& path, const std::vector<std::string>& skip,
               const std::vector<std::string>& transparent = {}) {
    Model model = ModelLoader::load(path, skip, transparent);
    std::printf("[modèle] %-44s : %zu parties\n", path.filename().string().c_str(),
                model.partCount());
    return model;
}

}  /* namespace */

LoadedHelicopter::LoadedHelicopter(const std::filesystem::path& dir) {
    /* Listes de nœuds à écarter au chargement :
       - skipBody : plans flous des rotors (blur/disc), doublons HDR des vitrages,
         et version flotteurs du train (on garde les patins, par défaut chez
         FlightGear).
       - skipRotor : uniquement les plans flous des rotors. */
    const std::vector<std::string> skipBody{"hdr", "blur", "disc", "flotteur", "barre"};
    const std::vector<std::string> skipRotor{"blur", "disc"};
    /* Pour les cadrans, on écarte en plus la vitre : c'est un disque sans texture
       posé devant le cadran. Faute de l'éclairer comme un vrai verre, elle cache
       le fond gradué et les aiguilles et apparaît comme un trou sombre. */
    const std::vector<std::string> skipGauge{"blur", "disc", "vitre"};
    /* Nœuds à rendre translucides : la verrière de cabine et les vitres. */
    const std::vector<std::string> glass{"verriere", "vitre"};

    m_fuselage  = loadPart(dir / "alouette.ac", skipBody, glass);
    m_interior  = loadPart(dir / "Interior/interior.ac", skipBody, glass);

    /* Planche de bord et ses cadrans (sans animation). */
    /* Planche de bord : on écarte aussi les capots (sur1..sur6) posés au-dessus
       de chaque cadran. Sans reflet de verre pour les adoucir, ces pare-soleil
       sombres ressortent comme de vilains rectangles noirs devant les
       instruments une fois ceux-ci bien visibles. */
    const std::vector<std::string> skipPanel{"blur", "disc", "sur"};
    m_panel = loadPart(dir / "Interior/Panel/panel.ac", skipPanel);
    for (const GaugeDef& def : GAUGES) {
        Gauge gauge;
        gauge.model  = loadPart(dir / def.file, skipGauge);
        gauge.offset = PANEL_OFFSET + fgToAssimp(def.fgOffset);
        m_gauges.push_back(std::move(gauge));
    }

    /* Pièces des rotors : moyeu et pale, principaux et de queue. Une seule pale
       est chargée par rotor, puis recopiée et tournée à l'affichage. */
    m_mainHub   = loadPart(dir / "Externals/MainRotor/mainrotor.ac", skipRotor);
    m_mainBlade = loadPart(dir / "Externals/MainRotor/blade.ac", skipRotor);
    m_tailHub   = loadPart(dir / "Externals/TailRotor/tailrotor.ac", skipRotor);
    m_tailBlade = loadPart(dir / "Externals/TailRotor/blade.ac", skipRotor);
}

void LoadedHelicopter::draw(Shader& shader, const mat4& base, float rotorAngle) const {
    /* Correction commune à tout l'appareil : demi-tour autour de l'axe vertical
       (le nez FlightGear est à l'opposé du nôtre) puis remontée pour poser les
       patins au sol. 'root' est la transformation de base de tout l'hélicoptère. */
    const mat4 correction = glm::translate(mat4(1.0f), vec3{0.0f, Y_OFFSET, 0.0f}) *
                            glm::rotate(mat4(1.0f), PI, vec3{0.0f, 1.0f, 0.0f});
    const mat4 root = base * correction;

    /* Petit raccourci : fixe la matrice du modèle puis dessine la pièce. */
    const auto drawModel = [&](const Model& model, const mat4& transform, Pass pass) {
        shader.setMat4("u_model", transform);
        model.draw(shader, pass);
    };

    /* Transformations des rotors, calculées une fois et réutilisées par les deux
       passes (opaque puis transparente). L'angle du rotor principal est fourni par
       l'application (sens horaire vu de dessus, comme sur l'Alouette II ; à l'arrêt,
       une pale est alignée sur l'axe de l'appareil).
       La correction de nez appliquée à 'root' (demi-tour autour de Y) place la pale 0
       vers l'arrière ; on ajoute donc un demi-tour au rotor pour qu'au parking une
       pale pointe vers l'avant et que les deux autres encadrent la sortie turbine. */
    const float mainAngle = rotorAngle + PI;
    const mat4  mainBase  = root * glm::translate(mat4(1.0f), MAIN_HUB) *
                           glm::rotate(mat4(1.0f), mainAngle, vec3{0.0f, 1.0f, 0.0f});
    /* Rotor de queue : son disque est vertical, on bascule donc le rotor de -90
       degrés autour de X avant de le faire tourner autour de son propre axe. Il est
       solidaire du rotor principal, d'où l'angle déduit par le rapport de vitesse
       (et de sens opposé, comme auparavant). */
    const float tailAngle = -rotorAngle * (TAIL_SPIN / MAIN_SPIN);
    const mat4  tailBase  = root * glm::translate(mat4(1.0f), TAIL_HUB) *
                           glm::rotate(mat4(1.0f), -HALF_PI, vec3{1.0f, 0.0f, 0.0f}) *
                           glm::rotate(mat4(1.0f), tailAngle, vec3{0.0f, 1.0f, 0.0f});

    /* Position d'une pale du rotor principal, répartie autour de l'axe (une pale
       unique recopiée et tournée), au niveau du plan rotor. */
    const auto mainBladeMat = [&](int k) {
        const float heading =
            static_cast<float>(k) * (TWO_PI / static_cast<float>(MAIN_BLADES));
        return mainBase * glm::rotate(mat4(1.0f), heading, vec3{0.0f, 1.0f, 0.0f}) *
               glm::translate(mat4(1.0f), vec3{0.0f, MAIN_BLADE_RISE, 0.0f});
    };

    /* Passe opaque : fuselage (hors vitrages), intérieur, planche, cadrans, moyeux
       et rotor de queue. Les pales du rotor principal et leur disque flou sont
       traités à part, en mélange, pour le fondu selon le régime. */
    drawModel(m_fuselage, root, Pass::Opaque);
    drawModel(m_interior, root, Pass::Opaque);
    drawModel(m_panel, root * glm::translate(mat4(1.0f), PANEL_OFFSET), Pass::Opaque);
    for (const Gauge& gauge : m_gauges) {
        drawModel(gauge.model, root * glm::translate(mat4(1.0f), gauge.offset), Pass::Opaque);
    }
    drawModel(m_mainHub, mainBase, Pass::Opaque);
    for (int k = 0; k < MAIN_BLADES; ++k) {
        drawModel(m_mainBlade, mainBladeMat(k), Pass::Opaque);
    }
    drawModel(m_tailHub, tailBase, Pass::Opaque);
    for (int k = 0; k < TAIL_BLADES; ++k) {
        const float heading =
            static_cast<float>(k) * (TWO_PI / static_cast<float>(TAIL_BLADES));
        const mat4 bladeMat = tailBase *
                              glm::rotate(mat4(1.0f), heading, vec3{0.0f, 1.0f, 0.0f}) *
                              glm::translate(mat4(1.0f), vec3{0.0f, TAIL_BLADE_RISE, 0.0f});
        drawModel(m_tailBlade, bladeMat, Pass::Opaque);
    }

    /* Passe transparente : les vitrages, dessinés en dernier. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    drawModel(m_fuselage, root, Pass::Transparent);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    /* --- Disque flou du rotor (mis en commentaire, à reprendre plus tard) -------
       Idée : à haut régime, estomper les pales distinctes (fondu sortant selon le
       régime) et les remplacer par un disque flou translucide, pour éviter l'effet
       stroboscopique. Les pales seraient alors dessinées dans la passe en mélange :

       const float bladeFade = 1.0f - clamp(rotorFraction, 0.0f, 1.0f);
       if (bladeFade > 0.01f) {
           for (int k = 0; k < MAIN_BLADES; ++k) {
               shader.setMat4("u_model", mainBladeMat(k));
               m_mainBlade.draw(shader, Pass::Opaque, bladeFade);  // opacityScale
           }
       }
       et l'application dessinerait le disque translucide via mainHubWorld() (voir
       le bloc correspondant, lui aussi en commentaire, dans Application.cpp).
       -------------------------------------------------------------------------- */
}

vec3 LoadedHelicopter::mainHubWorld(const mat4& base) const {
    /* Même chaîne de transformations que pour le rotor dans draw(), jusqu'au plan
       des pales : sert à poser le disque flou du rotor au bon endroit. */
    const mat4 correction = glm::translate(mat4(1.0f), vec3{0.0f, Y_OFFSET, 0.0f}) *
                            glm::rotate(mat4(1.0f), PI, vec3{0.0f, 1.0f, 0.0f});
    const mat4 hub = base * correction * glm::translate(mat4(1.0f), MAIN_HUB) *
                     glm::translate(mat4(1.0f), vec3{0.0f, MAIN_BLADE_RISE, 0.0f});
    return vec3(hub[3]);
}

}  /* namespace artouste::render */
