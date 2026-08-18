/*
 * LoadedHelicopterPilote.cpp
 * Pilote sur son siège : segments de bras et de jambes chargés à part pour
 * pouvoir les animer, et tenue selon la livrée.
 *
 * Le pilote entier sert aux vues externes. En vue cockpit on lui enlève la
 * tête, le torse et le haut des bras, qui masqueraient la planche de bord.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#include "render/LoadedHelicopter.hpp"

#include "render/LoadedHelicopterDetail.hpp"
#include "render/Model.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace artouste::render {

using namespace heli_detail;

void LoadedHelicopter::chargerPilote(const std::filesystem::path& dir) {
    const std::vector<std::string> skipBody{"hdr", "blur", "disc", "flotteur",
                                           "barre", "roue"};
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

    /* Tenue du pilote (chemise et casque) selon la livrée : chaque segment ci-
       dessus est un découpage indépendant du même general_pilot.ac/.png, donc
       chacun reçoit sa propre copie des trois textures de rechange (voir
       PilotSkin). La tête (m_pilot/m_pilotCockpit) porte le casque ; les autres
       segments ne portent que la chemise, mais le repeint est sans effet là où
       le segment ne couvre pas cette zone de l'atlas. */
    const std::filesystem::path pilotDir = dir / "Pilot";
    loadPilotSkin(pilotDir, m_pilot);
    loadPilotSkin(pilotDir, m_pilotCockpit);
    loadPilotSkin(pilotDir, m_legLeft);
    loadPilotSkin(pilotDir, m_legRight);
    loadPilotSkin(pilotDir, m_armUpper);
    loadPilotSkin(pilotDir, m_forearm);
    loadPilotSkin(pilotDir, m_grip);
    loadPilotSkin(pilotDir, m_armUpperLeft);
    loadPilotSkin(pilotDir, m_forearmLeft);
}

} /* namespace artouste::render */
