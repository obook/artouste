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

#include <cmath>
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

/* Position du pilote sur son siège (issue de Pilot/all-pilots.xml). Sur l'Alouette
   II le pilote est à droite : on prend donc le côté latéral négatif (la place
   "copilote" de FlightGear). Le pilote est purement décoratif : ses animations
   FlightGear (tête, bras, jambes suivant les commandes) sont ignorées, comme les
   aiguilles figées des cadrans. */
const vec3 PILOT_OFFSET = fgToAssimp(vec3{-3.97159f, -0.40060f, -0.54200f});

/* Copilote sur le siège gauche : même position que le pilote mais côté latéral
   opposé (c'est le placement d'origine du pilote chez FlightGear). */
const vec3 COPILOT_OFFSET = fgToAssimp(vec3{-3.97159f, 0.40060f, -0.54200f});

/* Palonniers (rudder) au sol, en avant des sièges (issus de all-pedals.xml). Une
   paire par siège ; leurs animations FlightGear (bascule au palonnier) sont
   ignorées, comme tout le reste, on les laisse fixes. */
const vec3 PEDALS_PILOT_OFFSET   = fgToAssimp(vec3{-4.46341f, -0.38800f, -0.83315f});
const vec3 PEDALS_COPILOT_OFFSET = fgToAssimp(vec3{-4.46341f, 0.38800f, -0.83315f});

/* Manche cyclique complet (modèle yokes/yoke.ac, colonne depuis le plancher) : une
   colonne par siège, devant le pilote (issus de all-yokes.xml). La poignée incluse
   dans le modèle pilote n'en est qu'un bout ; c'est ce yoke qui fait la vraie tige.
   Ses animations FlightGear sont ignorées, on le laisse fixe. */
const vec3 CYCLIC_PILOT_OFFSET   = fgToAssimp(vec3{-4.18685f, -0.38800f, -0.80400f});
const vec3 CYCLIC_COPILOT_OFFSET = fgToAssimp(vec3{-4.18685f, 0.38800f, -0.80400f});

/* Levier de collectif (modèle Interior/.../collective/collective.ac), un par siège,
   à gauche de chaque pilote : c'est la commande que la main gauche tire pour monter.
   Le modèle FlightGear le pose presque à plat au plancher, où il est masqué par le bas
   de cabine ; on le replace bien en vue, entre les sièges, et on le redresse (le vrai
   collectif monte en biais). Coordonnées dans le repère du modèle (avant correction) :
   x avant/arrière (négatif vers l'avant), y vertical, z latéral (négatif = côté droit,
   place du pilote). Le levier pivote autour de sa base et on l'anime au collectif. */
/* Levier de collectif, un par siège, à gauche de chaque pilote. Le modèle FlightGear
   le pose presque à plat au plancher, où il est masqué ; on le replace bien en vue,
   redressé (le vrai collectif monte en biais), et on l'anime avec la commande. */
const vec3 COLLECTIVE_PILOT_OFFSET   = vec3{-3.74f, -0.70f, -0.24f};  /* à gauche du pilote (siège droit) */
const vec3 COLLECTIVE_COPILOT_OFFSET = vec3{-3.74f, -0.70f, 0.24f};   /* à gauche du copilote (siège gauche) */
constexpr float COLLECTIVE_STAND_DEG = 30.0f;  /* redressement de base du levier (au repos) */
constexpr float COLLECTIVE_RAISE_DEG = 16.0f;  /* levée supplémentaire à plein collectif */
constexpr float COLLECTIVE_SCALE     = 1.3f;   /* léger grossissement pour la lisibilité */

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

/* Construit un décalque : un quad texturé dans le plan XY (face +Z), centré sur
   l'origine, côté 1 x 1, coordonnées de texture 0..1. Rangé comme une unique
   partie transparente d'un Model, pour réutiliser tout le rendu existant. */
Model makeDecal(const std::filesystem::path& texPath) {
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

/* Pose un décalque sur un flanc. 'fgPos' est la position de son centre (coordonnées
   FlightGear), 'w' et 'h' sa largeur et sa hauteur en mètres. Le quad regarde vers
   l'extérieur du côté correspondant (demi-tour pour le côté gauche, fg.y < 0), avec
   un léger décalage pour ne pas vibrer (z-fighting) contre la coque. */
void drawDecal(Shader& shader, const mat4& root, const Model& decal, const vec3& fgPos,
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
    /* Livrée Gendarmerie nationale : texture bleue de rechange, préchargée dans le
       cache du fuselage et activable à la demande (voir setGendarmerieLivery). */
    m_liveryGendarmerie = m_fuselage.acquireTexture(dir / "texture-gendarmerie.png");
    m_interior  = loadPart(dir / "Interior/interior.ac", skipBody, glass);

    /* Pilote sur son siège (modèle FlightGear, texture general_pilot.png). Le
       pilote entier sert aux vues externes (et au copilote). En vue cockpit, on
       enlève la tête (sinon l'intérieur du crâne masque tout), le torse (sinon la
       poitrine cache la planche), le haut des bras (brasG/brasD, en correspondance
       exacte pour ne pas emporter avantbrasG/avantbrasD) et les jambes, ces
       dernières étant chargées à part pour les animer au palonnier. */
    const std::vector<std::string> legParts{"cuisse", "jambe", "pied"};
    std::vector<std::string> skipCockpit{"tete", "casque", "corps", "=brasg", "=brasd"};
    skipCockpit.insert(skipCockpit.end(), legParts.begin(), legParts.end());
    /* Le bras droit (haut du bras, avant-bras, poignée) est chargé à part, en deux
       segments articulés (épaule, coude), pour suivre le manche cyclique sans se
       déformer. Le pilote cockpit ne garde que l'avant-bras gauche. */
    skipCockpit.push_back("avantbrasd");
    skipCockpit.push_back("manche");
    /* Le bras GAUCHE est aussi reconstruit (haut fixe + avant-bras articulé) pour que
       la main vienne se poser sur la poignée du collectif. On le retire donc du pilote
       entier comme du pilote cockpit, dans les deux cas il est redessiné à part. */
    skipCockpit.push_back("avantbrasg");
    std::vector<std::string> skipPilot = skipBody;
    skipPilot.push_back("=brasg");
    skipPilot.push_back("avantbrasg");
    m_pilot        = loadPart(dir / "Pilot/general_pilot.ac", skipPilot);
    m_pilotCockpit = loadPart(dir / "Pilot/general_pilot.ac", skipCockpit);
    /* Bras droit en trois segments : haut du bras (brasD), avant-bras (avantbrasD)
       et poignée (manche), chacun isolé. */
    m_armUpper = loadPart(dir / "Pilot/general_pilot.ac",
                          {"tete", "casque", "corps", "=brasg", "avantbras", "manche",
                           "cuisse", "jambe", "pied"});
    m_forearm  = loadPart(dir / "Pilot/general_pilot.ac",
                          {"tete", "casque", "corps", "=brasg", "=brasd", "avantbrasg",
                           "manche", "cuisse", "jambe", "pied"});
    m_grip     = loadPart(dir / "Pilot/general_pilot.ac",
                          {"tete", "casque", "corps", "=brasg", "=brasd", "avantbras",
                           "cuisse", "jambe", "pied"});

    /* Points d'articulation = jonctions réelles entre les maillages : la paire de
       sommets la plus proche entre deux pièces (le point où elles se touchent). Le
       coude relie le haut du bras à l'avant-bras, le poignet l'avant-bras à la
       poignée. Bien plus précis que les coins de boite englobante. */
    const auto junction = [](const Model& a, const Model& b) {
        float bestSq = 1e30f;
        vec3  result{0.0f};
        for (const vec3& pa : a.positions()) {
            for (const vec3& pb : b.positions()) {
                const vec3  d  = pa - pb;
                const float sq = glm::dot(d, d);
                if (sq < bestSq) {
                    bestSq = sq;
                    result = 0.5f * (pa + pb);
                }
            }
        }
        return result;
    };
    m_elbowLocal = junction(m_armUpper, m_forearm);
    m_wristLocal = junction(m_forearm, m_grip);

    /* Bras gauche en deux segments : haut du bras (brasG) et avant-bras (avantbrasG,
       qui inclut la main). Le coude est leur jonction réelle ; la main est le bout de
       l'avant-bras le plus éloigné du coude. */
    m_armUpperLeft = loadPart(dir / "Pilot/general_pilot.ac",
                              {"tete", "casque", "corps", "=brasd", "avantbras", "manche",
                               "cuisse", "jambe", "pied"});
    m_forearmLeft  = loadPart(dir / "Pilot/general_pilot.ac",
                              {"tete", "casque", "corps", "=brasg", "=brasd", "avantbrasd",
                               "manche", "cuisse", "jambe", "pied"});
    m_elbowLeftLocal = junction(m_armUpperLeft, m_forearmLeft);
    /* Main gauche = sommet de l'avant-bras le plus loin du coude. */
    {
        float best = -1.0f;
        for (const vec3& p : m_forearmLeft.positions()) {
            const float d = glm::length(p - m_elbowLeftLocal);
            if (d > best) {
                best = d;
                m_handLeftLocal = p;
            }
        }
    }

    /* Jambes isolées (gauche et droite) pour les faire pivoter au palonnier : on
       écarte tout sauf cuisse/jambe/pied du côté voulu. */
    m_legLeft  = loadPart(dir / "Pilot/general_pilot.ac",
                          {"tete", "casque", "corps", "=brasg", "=brasd", "avantbras",
                           "manche", "cuissed", "jambed", "piedd"});
    m_legRight = loadPart(dir / "Pilot/general_pilot.ac",
                          {"tete", "casque", "corps", "=brasg", "=brasd", "avantbras",
                           "manche", "cuisseg", "jambeg", "piedg"});

    /* Palonnier : pédales gauche (paloG) et droite (paloD) isolées, pour les faire
       basculer en sens opposé au palonnier. */
    m_pedalLeft  = loadPart(dir / "Interior/Panel/Instruments/pedals/pedals.ac", {"palod"});
    m_pedalRight = loadPart(dir / "Interior/Panel/Instruments/pedals/pedals.ac", {"palog"});
    /* Manche cyclique : la colonne complète, recopiée devant chaque siège. */
    m_cyclic = loadPart(dir / "Interior/Panel/Instruments/yokes/yoke.ac", skipBody);

    /* Levier de collectif, chargé en deux morceaux pour pouvoir animer le levier
       (qui pivote) sans bouger son embase (fixée au plancher). */
    const std::filesystem::path collectivePath =
        dir / "Interior/Panel/Instruments/collective/collective.ac";
    m_collectiveBase  = loadPart(collectivePath, {"collective"});  /* ne garde que l'embase */
    m_collectiveLever = loadPart(collectivePath, {"base"});        /* ne garde que le levier */
    /* Poignée = bout du levier le plus en avant (x le plus négatif dans son repère),
       là où la main gauche vient se poser. */
    {
        float minX = 1e30f;
        for (const vec3& p : m_collectiveLever.positions()) {
            if (p.x < minX) {
                minX = p.x;
                m_collectiveGripLocal = p;
            }
        }
    }

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

    /* Marquages de la livrée Gendarmerie (posés sur les flancs en 3D, voir draw). */
    m_decalGendarmerie = makeDecal(dir / "decal-gendarmerie.png");
    m_decalReg         = makeDecal(dir / "decal-fbrhp.png");
    m_decalStripe      = makeDecal(dir / "decal-stripe.png");
}

void LoadedHelicopter::setGendarmerieLivery(bool on) {
    m_gendarmerie = on;
    m_fuselage.setLivery(on ? m_liveryGendarmerie : nullptr);
}

void LoadedHelicopter::draw(Shader& shader, const mat4& base, float rotorAngle,
                            bool fullPilot, float rudder, float cyclicLong,
                            float cyclicLat, float collective) const {
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
    /* En vue cockpit, le pilote est dessiné sans jambes (chargées à part) : on
       affiche alors ses jambes animées au palonnier. Le pilote entier (vues
       externes) et le copilote gardent leurs jambes figées. */
    drawModel(fullPilot ? m_pilot : m_pilotCockpit,
              root * glm::translate(mat4(1.0f), PILOT_OFFSET), Pass::Opaque);
    drawModel(m_pilot, root * glm::translate(mat4(1.0f), COPILOT_OFFSET), Pass::Opaque);
    /* Inclinaison du manche cyclique (issue de yoke.xml) : tangage autour de l'axe
       latéral (notre Z), roulis autour de l'axe avant-arrière (notre X), pivot à la
       base du manche. La même rotation servira à faire suivre l'avant-bras droit. */
    const float cyclicPitch = glm::radians(cyclicLong * 15.0f);
    const float cyclicRoll  = glm::radians(cyclicLat * -10.0f);
    const mat4  cyclicR = glm::rotate(mat4(1.0f), cyclicPitch, vec3{0.0f, 0.0f, 1.0f}) *
                          glm::rotate(mat4(1.0f), cyclicRoll, vec3{1.0f, 0.0f, 0.0f});

    if (!fullPilot) {
        /* Jambes du pilote : pivot à la hanche (issu de pilot.xml), rotation autour
           de l'axe latéral (notre Z), opposée d'une jambe à l'autre, ~10 deg par
           unité de palonnier. */
        const vec3 hip       = fgToAssimp(vec3{0.237f, 0.0f, -0.065f});
        const float legAngle = glm::radians(rudder * -10.0f);
        const auto legMat = [&](float angle) {
            return root * glm::translate(mat4(1.0f), PILOT_OFFSET) *
                   glm::translate(mat4(1.0f), hip) *
                   glm::rotate(mat4(1.0f), angle, vec3{0.0f, 0.0f, 1.0f}) *
                   glm::translate(mat4(1.0f), -hip);
        };
        drawModel(m_legLeft, legMat(legAngle), Pass::Opaque);
        drawModel(m_legRight, legMat(-legAngle), Pass::Opaque);

        const mat4 pilotBase  = root * glm::translate(mat4(1.0f), PILOT_OFFSET);
        const mat4 stickXform = glm::translate(mat4(1.0f), CYCLIC_PILOT_OFFSET) * cyclicR *
                                glm::translate(mat4(1.0f), -CYCLIC_PILOT_OFFSET);

        /* Haut du bras : laissé au repos. Il reste rattaché à l'épaule et tient le
           coude en place (donc aucun gonflement). */
        drawModel(m_armUpper, pilotBase, Pass::Opaque);

        /* Poignée : elle suit rigidement le manche (même rotation autour de sa
           base), donc la prise reste parfaite. */
        drawModel(m_grip, root * stickXform * glm::translate(mat4(1.0f), PILOT_OFFSET),
                  Pass::Opaque);

        /* Avant-bras : il pivote autour du coude (fixe, donc rattaché au haut du
           bras) pour rejoindre le poignet, qui a suivi la poignée sur le manche. Un
           léger étirement le long de l'os lui permet d'atteindre exactement le
           poignet. Le bras reste connecté de l'épaule à la poignée. */
        const vec3 elbow  = PILOT_OFFSET + m_elbowLocal;
        const vec3 wrist0 = PILOT_OFFSET + m_wristLocal;
        /* Le poignet (jonction avant-bras/poignée) suit le manche avec la poignée :
           l'avant-bras y amène son extrémité -> raccord exact, sans trou. */
        const vec3 wrist1 = vec3(stickXform * vec4(wrist0, 1.0f));
        const float len0  = glm::length(wrist0 - elbow);
        const float len1  = glm::length(wrist1 - elbow);
        const vec3  d0    = (wrist0 - elbow) / glm::max(len0, 1e-4f);
        const vec3  d1    = (wrist1 - elbow) / glm::max(len1, 1e-4f);
        mat4        aim   = mat4(1.0f);
        const vec3  axis  = glm::cross(d0, d1);
        const float sinA  = glm::length(axis);
        if (sinA > 1e-4f) {
            aim = glm::rotate(mat4(1.0f), std::atan2(sinA, glm::dot(d0, d1)), axis / sinA);
        }
        const float stretch = len0 > 1e-4f ? len1 / len0 : 1.0f;
        const mat4  scale   = mat4(mat3(1.0f) + (stretch - 1.0f) * glm::outerProduct(d0, d0));
        const mat4  foreArm = pilotBase * glm::translate(mat4(1.0f), m_elbowLocal) * aim *
                              scale * glm::translate(mat4(1.0f), -m_elbowLocal);
        drawModel(m_forearm, foreArm, Pass::Opaque);
    }
    /* Palonniers devant chaque siège : les deux pédales basculent en sens opposé
       (~15 deg par unité de palonnier), autour de l'axe latéral. */
    const float pedalAngle = glm::radians(rudder * -15.0f);
    const auto pedalMat = [&](const vec3& offset, float angle) {
        return root * glm::translate(mat4(1.0f), offset) *
               glm::rotate(mat4(1.0f), angle, vec3{0.0f, 0.0f, 1.0f});
    };
    drawModel(m_pedalLeft, pedalMat(PEDALS_PILOT_OFFSET, pedalAngle), Pass::Opaque);
    drawModel(m_pedalRight, pedalMat(PEDALS_PILOT_OFFSET, -pedalAngle), Pass::Opaque);
    /* Pédales du copilote laissées au neutre : ses jambes, elles, sont figées, des
       pédales animées lui rentreraient dans les pieds. */
    drawModel(m_pedalLeft, pedalMat(PEDALS_COPILOT_OFFSET, 0.0f), Pass::Opaque);
    drawModel(m_pedalRight, pedalMat(PEDALS_COPILOT_OFFSET, 0.0f), Pass::Opaque);
    /* Manches cycliques devant chaque siège. Celui du pilote s'incline avec la
       commande (en vue cockpit, où la main suit) ; celui du copilote reste fixe,
       sa main à lui étant figée. */
    const mat4 pilotCyclic = (!fullPilot)
        ? root * glm::translate(mat4(1.0f), CYCLIC_PILOT_OFFSET) * cyclicR
        : root * glm::translate(mat4(1.0f), CYCLIC_PILOT_OFFSET);
    drawModel(m_cyclic, pilotCyclic, Pass::Opaque);
    drawModel(m_cyclic, root * glm::translate(mat4(1.0f), CYCLIC_COPILOT_OFFSET), Pass::Opaque);

    /* Levier de collectif à chaque siège : embase fixe au plancher, levier qui pivote
       autour de sa base. Il est modelé le long de l'axe X (poignée vers l'avant) ; on
       le redresse d'un angle de repos puis on le lève encore avec la commande, le tout
       en tournant autour de l'axe latéral (Z). Un angle négatif relève la poignée. */
    const float leverDeg = -(COLLECTIVE_STAND_DEG + COLLECTIVE_RAISE_DEG * clamp(collective, 0.0f, 1.0f));
    const mat4  leverRot = glm::rotate(mat4(1.0f), glm::radians(leverDeg), vec3{0.0f, 0.0f, 1.0f});
    /* Transformation du levier relative à son embase (grossissement puis pivot). */
    const mat4  leverLocal = glm::scale(mat4(1.0f), vec3{COLLECTIVE_SCALE}) * leverRot;

    struct Seat {
        vec3 pilot;       /* origine du pilote sur ce siège */
        vec3 collective;  /* origine du levier de collectif de ce siège */
    };
    const Seat seats[] = {
        {PILOT_OFFSET, COLLECTIVE_PILOT_OFFSET},
        {COPILOT_OFFSET, COLLECTIVE_COPILOT_OFFSET},
    };
    for (const Seat& s : seats) {
        /* Embase et levier du collectif. */
        const mat4 colAt = glm::translate(mat4(1.0f), s.collective);
        drawModel(m_collectiveBase, root * colAt * glm::scale(mat4(1.0f), vec3{COLLECTIVE_SCALE}),
                  Pass::Opaque);
        drawModel(m_collectiveLever, root * colAt * leverLocal, Pass::Opaque);

        /* Bras gauche : haut du bras laissé au repos (rattaché à l'épaule), avant-bras
           qui pivote au coude pour amener la main sur la poignée du levier. Même
           méthode que le bras droit sur le cyclique (rotation + léger étirement le long
           de l'os pour atteindre exactement la poignée, sans trou). */
        const mat4 pilotBaseL = root * glm::translate(mat4(1.0f), s.pilot);
        drawModel(m_armUpperLeft, pilotBaseL, Pass::Opaque);

        const vec3  gripModel = vec3(colAt * leverLocal * vec4(m_collectiveGripLocal, 1.0f));
        const vec3  elbow = s.pilot + m_elbowLeftLocal;
        const vec3  hand0 = s.pilot + m_handLeftLocal;          /* main au repos */
        const vec3  hand1 = gripModel;                          /* poignée du collectif */
        const float len0  = glm::length(hand0 - elbow);
        const float len1  = glm::length(hand1 - elbow);
        const vec3  d0    = (hand0 - elbow) / glm::max(len0, 1e-4f);
        const vec3  d1    = (hand1 - elbow) / glm::max(len1, 1e-4f);
        mat4        aim   = mat4(1.0f);
        const vec3  axis  = glm::cross(d0, d1);
        const float sinA  = glm::length(axis);
        if (sinA > 1e-4f) {
            aim = glm::rotate(mat4(1.0f), std::atan2(sinA, glm::dot(d0, d1)), axis / sinA);
        }
        const float stretch = len0 > 1e-4f ? len1 / len0 : 1.0f;
        const mat4  scaleM  = mat4(mat3(1.0f) + (stretch - 1.0f) * glm::outerProduct(d0, d0));
        const mat4  foreArmL = pilotBaseL * glm::translate(mat4(1.0f), m_elbowLeftLocal) * aim *
                               scaleM * glm::translate(mat4(1.0f), -m_elbowLeftLocal);
        drawModel(m_forearmLeft, foreArmL, Pass::Opaque);
    }

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

    /* Passe transparente : marquages de livrée (posés sur la coque opaque) puis
       vitrages, dessinés en dernier. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    if (m_gendarmerie) {
        /* "GENDARMERIE" sur le bas des flancs de cabine, "F-BRHP" sur le caisson
           arrière. Positions en coordonnées FlightGear, affinées visuellement. */
        /* Liséré blanc juste au-dessus du mot, sur chaque flanc. */
        drawDecal(shader, root, m_decalStripe, vec3{-3.60f, 0.90f, -0.71f}, 1.15f, 0.015f);
        drawDecal(shader, root, m_decalStripe, vec3{-3.60f, -0.90f, -0.71f}, 1.15f, 0.015f);
        drawDecal(shader, root, m_decalGendarmerie, vec3{-3.60f, 0.90f, -0.80f}, 0.90f, 0.13f);
        drawDecal(shader, root, m_decalGendarmerie, vec3{-3.60f, -0.90f, -0.80f}, 0.90f, 0.13f);
        drawDecal(shader, root, m_decalReg, vec3{-1.92f, 0.62f, -0.05f}, 0.36f, 0.11f);
        drawDecal(shader, root, m_decalReg, vec3{-1.92f, -0.62f, -0.05f}, 0.36f, 0.11f);
    }
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
