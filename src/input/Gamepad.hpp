/*
 * Gamepad.hpp
 * Lecture des commandes à la manette (API gamepad de GLFW / mapping SDL).
 * Répartition des commandes :
 *   stick gauche    -> cyclique (tangage / roulis)
 *   stick droit X   -> palonniers
 *   gâchettes RT/LT -> collectif (levier : RT monte, LT descend, garde la position)
 *   bouton Y        -> change de vue
 *   bouton Start    -> démarre ou coupe la turbine
 *   bouton B        -> affiche ou masque le HUD
 *   bouton X        -> replace l'appareil au point de départ
 *   bouton Back     -> met en pause ou reprend
 *   croix droite    -> allume ou coupe la radio internet
 *   croix haut/bas  -> balance radio / hélico (haut vers la radio, bas vers l'hélico)
 *   LB + RB         -> retour au menu (combinaison, pour éviter les retours accidentels)
 *   LB              -> bascule le mode assisté (L1 sur PS4/PS5), sauf combiné à RB
 *                       (retour au menu, voir plus haut)
 *   RB              -> bascule l'atterrissage automatique (R1 sur PS4/PS5), sauf
 *                       combiné à LB (retour au menu, voir plus haut)
 *   L3 (clic stick gauche) -> regard du pilote en vue cockpit : tenir L3 et pousser
 *                       le stick droit fait tourner la tête (gauche/droite) et la
 *                       lever ou la baisser (haut/bas) ; la déflexion commande une
 *                       vitesse de rotation, l'angle reste acquis stick au neutre.
 *                       État maintenu, relâcher ramène la vue vers l'avant. Pendant
 *                       le regard, le stick droit ne commande plus les palonniers.
 *   R3 (clic stick droit) -> tir (mitrailleuse), mode zombie uniquement -- état
 *                       maintenu (pas un front montant comme les autres boutons) ;
 *                       ancien déclencheur de l'atterrissage automatique, libéré
 *                       depuis son passage sur RB
 * La manette conserve un état d'une image à l'autre : la position du levier de
 * collectif et l'état précédent des boutons (pour détecter le moment de l'appui).
 *
 * Implémentation répartie sur trois fichiers : Gamepad.cpp porte le cycle de
 * vie (mappings, connexion) et la détection d'activité ; GamepadAxes.cpp les
 * axes de vol (poll) ; GamepadButtons.cpp la détection de front des boutons.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/Controls.hpp"

#include <filesystem>

struct GLFWgamepadstate;

namespace artouste::input {

class Gamepad {
public:
    /* Charge la base communautaire de mappings SDL (gamecontrollerdb.txt) depuis
     * le dossier des ressources, pour reconnaître les manettes absentes de la base
     * intégrée de GLFW -- dont les modèles Xbox récents en Bluetooth (product 0B13),
     * qui n'y figurent pas encore. Charge ensuite gamecontrollerdb-extra.txt, qui
     * corrige ou complète cette base sans la modifier (voir son entête). À appeler
     * une fois, juste après glfwInit. Silencieux si un fichier est absent : GLFW
     * conserve alors sa base intégrée, qui couvre déjà les manettes courantes. */
    static void loadMappings(const std::filesystem::path& assetDir) noexcept;

    /* Manette branchée ET reconnue (mapping SDL disponible) ? */
    [[nodiscard]] static bool isPresent() noexcept;

    /* La manette est-elle nettement sollicitée ? Sert à choisir la source de
     * commande active (clavier ou manette). */
    [[nodiscard]] static bool isActive() noexcept;

    /* Vide la file d'événements de la manette, sans rien en faire.
     *
     * À appeler pendant toute opération bloquante de plusieurs secondes (le
     * chargement d'une carte). Sans lecture, le tampon evdev du client déborde,
     * le noyau jette des événements et signale SYN_DROPPED. GLFW le traite mais
     * ne resynchronise alors que les axes (pollAbsState / EVIOCGABS) : il ne
     * relit jamais les boutons, faute d'EVIOCGKEY. Un bouton relâché pendant le
     * trou reste donc vu comme enfoncé jusqu'au prochain événement le concernant,
     * et le premier appui suivant ne produit plus de front. */
    static void viderFile() noexcept;

    /* À brancher sur glfwSetJoystickCallback. À la déconnexion d'une manette,
     * demande la remise à zéro du levier de collectif, appliquée au prochain
     * poll (donc à la reconnexion). */
    static void onJoystickEvent(int jid, int event) noexcept;

    /* Met à jour et renvoie les commandes pour ce pas de temps (dt en secondes).
     * Le collectif est un levier : il garde sa position quand on relâche.
     * Définie dans GamepadAxes.cpp. */
    [[nodiscard]] physics::Controls poll(float dt) noexcept;

    /* Vrai une seule fois, au moment où le bouton Y vient d'être pressé.
     * Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool viewTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton Start vient d'être pressé.
     * Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool turbineTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton B vient d'être pressé (HUD).
     * Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool hudTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton Back vient d'être pressé (pause).
     * Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool pauseTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton X vient d'être pressé (reset).
     * Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool resetPressed() noexcept;

    /* Vrai une seule fois, au moment où la combinaison LB + RB vient d'être pressée
     * (retour au menu de démarrage). Une combinaison évite les retours accidentels.
     * Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool menuPressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton A vient d'être pressé (fait
     * défiler la livrée, comme la touche L). Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool liveryTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où LB (L1 sur PS4/PS5) vient d'être pressée
     * seule (bascule le mode assisté, comme la touche M) -- pas si RB est aussi
     * tenu, cette combinaison étant réservée au retour au menu (menuPressed).
     * Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool assistTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où RB (R1 sur PS4/PS5) vient d'être pressée
     * seule (bascule l'atterrissage automatique, comme la touche J) -- pas si LB
     * est aussi tenu, cette combinaison étant réservée au retour au menu
     * (menuPressed). Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool autolandTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où la croix directionnelle droite vient d'être
     * pressée (allume ou coupe la radio internet, comme la touche K).
     * Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool radioTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où la croix directionnelle haut (ou bas) vient
     * d'être pressée : balance radio/hélico vers la radio (ou vers l'hélico), comme
     * les touches + et -. Un cran par appui, comme au clavier (GLFW ne répète pas).
     * Définies dans GamepadButtons.cpp. */
    [[nodiscard]] bool radioMixUpPressed() noexcept;
    [[nodiscard]] bool radioMixDownPressed() noexcept;

    /* R3 (clic du stick droit) est-il actuellement tenu ? État maintenu (pas un
       front montant) : le tir dure tant que le bouton est enfoncé, mode zombie
       uniquement (voir CombatMode/Weapon). Définie dans GamepadButtons.cpp. */
    [[nodiscard]] bool fireHeld() const noexcept;

    /* L3 (clic du stick gauche) est-il actuellement tenu ? État maintenu comme
       fireHeld : le regard dure tant que le bouton est enfoncé. Définie dans
       GamepadButtons.cpp. */
    [[nodiscard]] bool lookHeld() const noexcept;

    /* Commande de regard : position du stick droit X (zone morte et expansion
       comprises) tant que L3 est tenu, 0 sinon. Commande la vitesse de rotation en
       lacet de la tête du pilote en vue cockpit. Définie dans GamepadAxes.cpp. */
    [[nodiscard]] float lookAxis() const noexcept;

    /* Commande de regard vertical : position du stick droit Y (zone morte et
       expansion comprises, orientée "stick vers le haut = +1") tant que L3 est tenu,
       0 sinon. Commande la vitesse de rotation en tangage de la tête du pilote.
       Définie dans GamepadAxes.cpp. */
    [[nodiscard]] float lookAxisVertical() const noexcept;

    /* Remet le levier de collectif à zéro. */
    void reset() noexcept { m_collective = 0.0f; }

    /* Recale le levier de collectif mémorisé sur value (voir Keyboard::setCollective,
       même besoin de resynchronisation après un vol automatique). */
    void setCollective(float value) noexcept { m_collective = value; }

    /* Amorce les états de front des boutons avec l'état courant de la manette : un
     * bouton déjà tenu ne comptera pas comme un nouvel appui à la prochaine lecture.
     * À appeler en entrant en vol depuis le menu (le menu valide avec A, qui sert
     * aussi à défiler la livrée en vol : sans amorçage, le A de validation encore
     * tenu ferait défiler la livrée dès la première image). Définie dans
     * GamepadButtons.cpp. */
    void primeButtons() noexcept;

private:
    /* Charge un fichier de mappings SDL s'il existe. Utilisée par loadMappings
     * pour enchaîner la base communautaire puis les correctifs maison ; définie
     * dans Gamepad.cpp. */
    static void chargerFichierMappings(const std::filesystem::path& fichier) noexcept;

    /* Premier identifiant GLFW d'une manette reconnue (mapping SDL disponible),
     * ou -1 si aucune. Partagé par Gamepad.cpp, GamepadAxes.cpp et
     * GamepadButtons.cpp ; défini dans Gamepad.cpp. */
    static int activePad() noexcept;

    /* Lit l'état courant de la première manette reconnue dans 'state'. Renvoie
     * faux si aucune manette n'est branchée. Partagée par les trois fichiers
     * d'implémentation ; définie dans Gamepad.cpp. */
    static bool readState(GLFWgamepadstate& state) noexcept;

    /* Levé par onJoystickEvent (Gamepad.cpp) à la déconnexion d'une manette,
     * consommé par poll (GamepadAxes.cpp) pour remettre le levier de collectif
     * à zéro. Définie dans Gamepad.cpp. */
    static bool s_collectiveResetRequested;

    float m_collective = 0.0f;      /* position mémorisée du levier de collectif */
    bool m_prevY = false;           /* état du bouton Y à l'image précédente */
    bool m_prevStart = false;       /* état du bouton Start à l'image précédente */
    bool m_prevB = false;           /* état du bouton B à l'image précédente */
    bool m_prevBack = false;        /* état du bouton Back à l'image précédente */
    bool m_prevX = false;           /* état du bouton X à l'image précédente */
    bool m_prevMenu = false;        /* état de la combinaison LB + RB à l'image précédente */
    bool m_prevA = false;           /* état du bouton A à l'image précédente */
    bool m_prevLeftBumper = false;  /* état de LB à l'image précédente */
    bool m_prevRightBumper = false; /* état de RB à l'image précédente */
    bool m_prevDpadRight = false;   /* état de la croix droite à l'image précédente */
    bool m_prevDpadUp = false;      /* état de la croix haut à l'image précédente */
    bool m_prevDpadDown = false;    /* état de la croix bas à l'image précédente */
};

} /* namespace artouste::input */
