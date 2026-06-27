/*
 * LoadedHelicopter.cpp
 * Construction de l'hélicoptère chargé : assemblage des pièces (fuselage,
 * intérieur, pilote et ses bras/jambes articulés, commandes, rotors, décalques)
 * à partir des fichiers FlightGear. Le chargement des instruments animés et le
 * dessin sont dans LoadedHelicopterInstruments.cpp et LoadedHelicopterDraw.cpp.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#include "render/LoadedHelicopter.hpp"

#include "render/LoadedHelicopterDetail.hpp"
#include "render/Shader.hpp"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace artouste::render {

using namespace heli_detail;

LoadedHelicopter::LoadedHelicopter(const std::filesystem::path& dir) {
    /* Listes de nœuds à écarter au chargement :
       - skipBody : plans flous des rotors (blur/disc), doublons HDR des vitrages,
         version flotteurs du train (on garde les patins, par défaut chez FlightGear),
         et les roues (roueD/roueG) que l'on ne veut pas afficher.
       - skipRotor : uniquement les plans flous des rotors. */
    const std::vector<std::string> skipBody{"hdr", "blur", "disc", "flotteur", "barre", "roue"};
    const std::vector<std::string> skipRotor{"blur", "disc"};
    /* Nœuds à rendre translucides : la verrière de cabine et les vitres de porte.
       On ne vise pas "tourvitre" : c'est un maillage qui mêle le vitrage avant ET la
       partie pleine au niveau du tableau de bord ; le laisser opaque évite que cette
       partie pleine devienne un trou transparent. (Le vrai correctif serait de scinder
       ce maillage dans le modèle pour ne rendre transparent que le verre.) */
    const std::vector<std::string> glass{"verriere", "vitreporte"};

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
    /* Main gauche = sommet de l'avant-bras le plus loin du coude (le bout des doigts).
       On garde ce point de référence, dont la distance au coude correspond à la
       longueur réelle de l'avant-bras : forearmTransform n'étire alors quasiment pas
       l'os (pas de main déformée). Le réglage de hauteur se fait sur la cible de prise
       (voir gripModel dans LoadedHelicopterDraw), pas sur ce point. */
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
       là où la main gauche vient se poser. C'est bien le pommeau qui est saisi : c'est
       la PAUME (et non le bout des doigts) qui y est ancrée, voir m_handLeftLocal. */
    {
        float minX = 1e30f;
        for (const vec3& p : m_collectiveLever.positions()) {
            if (p.x < minX) {
                minX = p.x;
                m_collectiveGripLocal = p;
            }
        }
    }

    /* Planche de bord : on écarte aussi les capots (sur1..sur6) posés au-dessus
       de chaque cadran. Sans reflet de verre pour les adoucir, ces pare-soleil
       sombres ressortent comme de vilains rectangles noirs devant les
       instruments une fois ceux-ci bien visibles. */
    const std::vector<std::string> skipPanel{"blur", "disc", "sur"};
    m_panel = loadPart(dir / "Interior/Panel/panel.ac", skipPanel);
    for (const GaugeDef& def : GAUGES) {
        Gauge gauge;
        gauge.model  = loadPart(dir / def.file, {"blur", "disc", "vitre"});
        gauge.offset = PANEL_OFFSET + fgToAssimp(def.fgOffset);
        m_gauges.push_back(std::move(gauge));
    }

    /* Instruments animés du tableau de bord (chargés à part, voir
       LoadedHelicopterInstruments.cpp). */
    loadInstruments(dir);

    /* Pièces des rotors : moyeu et pale, principaux et de queue. Une seule pale
       est chargée par rotor, puis recopiée et tournée à l'affichage. */
    m_mainHub   = loadPart(dir / "Externals/MainRotor/mainrotor.ac", skipRotor);
    m_mainBlade = loadPart(dir / "Externals/MainRotor/blade.ac", skipRotor);
    m_tailHub   = loadPart(dir / "Externals/TailRotor/tailrotor.ac", skipRotor);
    m_tailBlade = loadPart(dir / "Externals/TailRotor/blade.ac", skipRotor);
    /* Livrée Gendarmerie des pales de queue : texture de rechange (jaune zébré
       rouge), préchargée dans le cache de la pale et activée à la demande. */
    m_tailBladeLivery =
        m_tailBlade.acquireTexture(dir / "Externals/TailRotor/tailrotor-gendarmerie.png");

    /* Arceau de protection du rotor de queue : isolé de la structure du fuselage
       (voir tools/livree) pour pouvoir le peindre en jaune en livrée Gendarmerie.
       Le fuselage lui-même (alouette.ac) ne le contient donc plus. */
    m_tailGuard        = loadPart(dir / "tailguard.ac", {});
    m_tailGuardLivery  = m_tailGuard.acquireTexture(dir / "tailguard-gendarmerie.png");
    m_tailGuardOrigine = m_tailGuard.acquireTexture(dir / "tailguard-origine.png");

    /* Marquages de la livrée Gendarmerie (posés sur les flancs en 3D, voir draw). */
    m_decalGendarmerie = makeDecal(dir / "decal-gendarmerie.png");
    m_decalReg         = makeDecal(dir / "decal-fbrhp.png");
    m_decalStripe      = makeDecal(dir / "decal-stripe.png");
}

void LoadedHelicopter::setGendarmerieLivery(bool on) {
    m_gendarmerie = on;
    m_fuselage.setLivery(on ? m_liveryGendarmerie : nullptr);
    m_tailBlade.setLivery(on ? m_tailBladeLivery : nullptr);
    /* L'arceau n'a pas de teinte d'origine satisfaisante dans sa texture (UV chaudes) :
       on force donc une couleur unie dans les deux cas, jaune en Gendarmerie, gris
       métal sinon (cohérent avec la pale de queue et le corps en livrée d'origine). */
    m_tailGuard.setLivery(on ? m_tailGuardLivery : m_tailGuardOrigine);
}

void LoadedHelicopter::drawModel(Shader& shader, const Model& model, const mat4& transform,
                                 Pass pass) const {
    shader.setMat4("u_model", transform);
    model.draw(shader, pass);
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
