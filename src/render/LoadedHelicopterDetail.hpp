/*
 * LoadedHelicopterDetail.hpp
 * Constantes et petits utilitaires partagés par les unités de compilation de
 * LoadedHelicopter (construction, instruments, dessin) : positions issues des
 * fichiers d'assemblage FlightGear, conversion de repère, chargement d'une pièce
 * et fabrication/pose d'un décalque. Regroupés ici en éléments "inline" (C++17)
 * pour une seule définition partagée entre les .cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "render/Model.hpp"
#include "render/ModelLoader.hpp"
#include "render/Shader.hpp"
#include "util/Math.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace artouste::render::heli_detail {

/* Correction de repère FlightGear vers notre moteur. */
inline constexpr float Y_OFFSET = 1.951f;  /* remonte les patins au niveau du sol */

/* Position des rotors (dans le repère du modèle, avant correction). Ces valeurs
   proviennent des fichiers d'assemblage de FlightGear. Une coordonnée
   FlightGear (x, y, z) correspond chez nous à (x, z, y). */
inline const vec3      MAIN_HUB{-2.142f, 0.177f, 0.000f};
inline constexpr float MAIN_BLADE_RISE = 0.826f;
inline constexpr int   MAIN_BLADES     = 3;

inline const vec3      TAIL_HUB{3.963f, 0.174f, 0.226f};
inline constexpr float TAIL_BLADE_RISE = 0.031f;
inline constexpr int   TAIL_BLADES     = 2;

/* Vitesses de rotation purement décoratives (en radians par seconde). Elles sont
   volontairement ralenties pour éviter l'effet stroboscopique. Le rotor de queue,
   bipale, tourne environ 3,5 fois plus vite que le rotor principal : moins que le
   rapport réel (~5,9x), mais au-delà il franchit le seuil stroboscopique (90 deg par
   image à 60 fps) et paraît ralentir, surtout quand la cadence baisse en vol. */
inline constexpr float MAIN_SPIN = 15.0f;
inline constexpr float TAIL_SPIN = 52.5f;  /* 3,5 x MAIN_SPIN */

/* Convertit une position exprimée dans le repère FlightGear (x avant/arrière,
   y latéral, z vertical) vers le repère du modèle (x, y vertical, z latéral). */
inline vec3 fgToAssimp(const vec3& fg) {
    return vec3{fg.x, fg.z, fg.y};
}

/* Position de la planche de bord (issue des fichiers d'assemblage FlightGear). */
inline const vec3 PANEL_OFFSET = fgToAssimp(vec3{-4.136f, 0.0f, -0.344f});

/* Position du pilote sur son siège (issue de Pilot/all-pilots.xml). Sur l'Alouette
   II le pilote est à droite : on prend donc le côté latéral négatif (la place
   "copilote" de FlightGear). Le pilote est purement décoratif : ses animations
   FlightGear (tête, bras, jambes suivant les commandes) sont ignorées, comme les
   aiguilles figées des cadrans. */
inline const vec3 PILOT_OFFSET = fgToAssimp(vec3{-3.97159f, -0.40060f, -0.54200f});

/* Copilote sur le siège gauche : même position que le pilote mais côté latéral
   opposé (c'est le placement d'origine du pilote chez FlightGear). */
inline const vec3 COPILOT_OFFSET = fgToAssimp(vec3{-3.97159f, 0.40060f, -0.54200f});

/* Palonniers (rudder) au sol, en avant des sièges (issus de all-pedals.xml). Une
   paire par siège ; leurs animations FlightGear (bascule au palonnier) sont
   ignorées, comme tout le reste, on les laisse fixes. */
inline const vec3 PEDALS_PILOT_OFFSET   = fgToAssimp(vec3{-4.46341f, -0.38800f, -0.83315f});
inline const vec3 PEDALS_COPILOT_OFFSET = fgToAssimp(vec3{-4.46341f, 0.38800f, -0.83315f});

/* Manche cyclique complet (modèle yokes/yoke.ac, colonne depuis le plancher) : une
   colonne par siège, devant le pilote (issus de all-yokes.xml). La poignée incluse
   dans le modèle pilote n'en est qu'un bout ; c'est ce yoke qui fait la vraie tige.
   Ses animations FlightGear sont ignorées, on le laisse fixe. */
inline const vec3 CYCLIC_PILOT_OFFSET   = fgToAssimp(vec3{-4.18685f, -0.38800f, -0.80400f});
inline const vec3 CYCLIC_COPILOT_OFFSET = fgToAssimp(vec3{-4.18685f, 0.38800f, -0.80400f});

/* Levier de collectif, un par siège, à gauche de chaque pilote. Le modèle FlightGear
   le pose presque à plat au plancher, où il est masqué ; on le replace bien en vue,
   redressé (le vrai collectif monte en biais), et on l'anime avec la commande.
   Coordonnées dans le repère du modèle (avant correction) : x avant/arrière (négatif
   vers l'avant), y vertical, z latéral (négatif = côté droit, place du pilote). */
inline const vec3      COLLECTIVE_PILOT_OFFSET   = vec3{-3.74f, -0.70f, -0.24f};  /* à gauche du pilote (siège droit) */
inline const vec3      COLLECTIVE_COPILOT_OFFSET = vec3{-3.74f, -0.70f, 0.24f};   /* à gauche du copilote (siège gauche) */
inline constexpr float COLLECTIVE_STAND_DEG = 30.0f;  /* redressement de base du levier (au repos) */
inline constexpr float COLLECTIVE_RAISE_DEG = 16.0f;  /* levée supplémentaire à plein collectif */
inline constexpr float COLLECTIVE_SCALE     = 1.3f;   /* léger grossissement pour la lisibilité */

/* Description des cadrans encore figés de la planche de bord : le fichier .ac de
   chacun et sa position relative au panneau (coordonnées FlightGear). Les instruments
   animés (ai, alt, vsi, hi, asi, torque) sont chargés et dessinés à part ; ne restent
   ici que vor et rmi (radionavigation non modélisée, aiguilles figées). */
struct GaugeDef {
    const char* file;
    vec3        fgOffset;
};
inline const GaugeDef GAUGES[] = {
    {"Interior/Panel/Instruments/vor/vor.ac", vec3{-0.223f, -0.181f, -0.014f}},
    {"Interior/Panel/Instruments/rmi/rmi.ac", vec3{-0.224f, 0.060f, -0.014f}},
};

/* Vario (VSI) : convertit une vitesse verticale (en ft/min) en angle de rotation de
   l'aiguille (en degrés). Le cadran n'est pas linéaire : la table ci-dessous a été
   relevée sur la face vsi.png (0 a 9 heures, montée vers le haut, descente vers le
   bas). On interpole linéairement entre les points, la descente est symétrique de la
   montée (angle de signe opposé), et au-dela de la pleine échelle l'aiguille reste
   contre la butée (+/- 2500 ft/min). */
inline float vsiNeedleAngleDeg(float fpm) {
    static const float kFpm[]   = {0.0f, 500.0f, 1000.0f, 2000.0f, 2500.0f};
    static const float kAngle[] = {0.0f, 36.0f,  66.0f,   116.0f,  151.0f};
    const int   count = 5;
    const float mag   = std::fabs(fpm);
    float       angle = kAngle[count - 1];  /* pleine échelle par défaut (butée) */
    for (int i = 0; i < count - 1; ++i) {
        if (mag <= kFpm[i + 1]) {
            const float t = (mag - kFpm[i]) / (kFpm[i + 1] - kFpm[i]);
            angle = kAngle[i] + t * (kAngle[i + 1] - kAngle[i]);
            break;
        }
    }
    return fpm < 0.0f ? -angle : angle;
}

/* Charge une pièce du modèle et affiche un petit compte-rendu (nom du fichier et
   nombre de parties obtenues), pratique pour vérifier le chargement. */
inline Model loadPart(const std::filesystem::path& path, const std::vector<std::string>& skip,
                      const std::vector<std::string>& transparent = {}) {
    Model model = ModelLoader::load(path, skip, transparent);
    std::printf("[modèle] %-44s : %zu parties\n", path.filename().string().c_str(),
                model.partCount());
    return model;
}

/* Construit un décalque : un quad texturé dans le plan XY (face +Z), centré sur
   l'origine, côté 1 x 1, coordonnées de texture 0..1. Rangé comme une unique
   partie transparente d'un Model, pour réutiliser tout le rendu existant. */
inline Model makeDecal(const std::filesystem::path& texPath) {
    std::vector<Vertex> verts(4);
    verts[0].position = vec3{-0.5f, -0.5f, 0.0f}; verts[0].uv = vec2{0.0f, 0.0f};
    verts[1].position = vec3{ 0.5f, -0.5f, 0.0f}; verts[1].uv = vec2{1.0f, 0.0f};
    verts[2].position = vec3{ 0.5f,  0.5f, 0.0f}; verts[2].uv = vec2{1.0f, 1.0f};
    verts[3].position = vec3{-0.5f,  0.5f, 0.0f}; verts[3].uv = vec2{0.0f, 1.0f};
    for (Vertex& v : verts) {
        v.normal = vec3{0.0f, 0.0f, 1.0f};
        v.color  = vec3{1.0f};
    }
    const std::vector<unsigned int> idx{0, 1, 2, 0, 2, 3};
    Model model;
    const Texture* tex = model.acquireTexture(texPath);
    model.addPart(Mesh(verts, idx), tex, /*transparent=*/true);
    return model;
}

/* Transformation d'un avant-bras articulé : il pivote autour du coude (point fixe
   rattaché au haut du bras) pour amener son extrémité de sa position de repos 'end0'
   vers la cible 'end1' (poignée du manche cyclique ou du collectif), avec un léger
   étirement le long de l'os pour atteindre exactement la cible sans laisser de trou.
   'pilotBase' est la pose du pilote, 'elbowLocal' le coude dans le repère du modèle
   pilote, et 'elbow' ce même coude exprimé dans le repère du pilote. */
inline mat4 forearmTransform(const mat4& pilotBase, const vec3& elbowLocal, const vec3& elbow,
                             const vec3& end0, const vec3& end1) {
    const float len0 = glm::length(end0 - elbow);
    const float len1 = glm::length(end1 - elbow);
    const vec3  d0   = (end0 - elbow) / glm::max(len0, 1e-4f);
    const vec3  d1   = (end1 - elbow) / glm::max(len1, 1e-4f);
    mat4        aim  = mat4(1.0f);
    const vec3  axis = glm::cross(d0, d1);
    const float sinA = glm::length(axis);
    if (sinA > 1e-4f) {
        aim = glm::rotate(mat4(1.0f), std::atan2(sinA, glm::dot(d0, d1)), axis / sinA);
    }
    const float stretch = len0 > 1e-4f ? len1 / len0 : 1.0f;
    const mat4  scale   = mat4(mat3(1.0f) + (stretch - 1.0f) * glm::outerProduct(d0, d0));
    return pilotBase * glm::translate(mat4(1.0f), elbowLocal) * aim * scale *
           glm::translate(mat4(1.0f), -elbowLocal);
}

/* Pose un décalque sur un flanc. 'fgPos' est la position de son centre (coordonnées
   FlightGear), 'w' et 'h' sa largeur et sa hauteur en mètres. Le quad regarde vers
   l'extérieur du côté correspondant (demi-tour pour le côté gauche, fg.y < 0), avec
   un léger décalage pour ne pas vibrer (z-fighting) contre la coque. */
inline void drawDecal(Shader& shader, const mat4& root, const Model& decal, const vec3& fgPos,
                      float w, float h) {
    vec3 pos = fgToAssimp(fgPos);
    pos.z += (fgPos.y >= 0.0f ? 0.02f : -0.02f);
    mat4 local = glm::translate(mat4(1.0f), pos);
    if (fgPos.y < 0.0f) {
        local = local * glm::rotate(mat4(1.0f), PI, vec3{0.0f, 1.0f, 0.0f});
    }
    const mat4 m = root * local * glm::scale(mat4(1.0f), vec3{w, h, 1.0f});
    shader.setMat4("u_model", m);
    decal.draw(shader, Pass::Transparent);
}

}  /* namespace artouste::render::heli_detail */
