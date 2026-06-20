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
 *   LB + RB         -> quitte (combinaison, pour éviter les sorties accidentelles)
 * La manette conserve un état d'une image à l'autre : la position du levier de
 * collectif et l'état précédent des boutons (pour détecter le moment de l'appui).
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/Controls.hpp"

namespace artouste::input {

class Gamepad {
public:
    /* Manette branchée ET reconnue (mapping SDL disponible) ? */
    [[nodiscard]] static bool isPresent() noexcept;

    /* La manette est-elle nettement sollicitée ? Sert à choisir la source de
     * commande active (clavier ou manette). */
    [[nodiscard]] static bool isActive() noexcept;

    /* Met à jour et renvoie les commandes pour ce pas de temps (dt en secondes).
     * Le collectif est un levier : il garde sa position quand on relâche. */
    [[nodiscard]] physics::Controls poll(float dt) noexcept;

    /* Vrai une seule fois, au moment où le bouton Y vient d'être pressé. */
    [[nodiscard]] bool viewTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton Start vient d'être pressé. */
    [[nodiscard]] bool turbineTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton B vient d'être pressé (HUD). */
    [[nodiscard]] bool hudTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton Back vient d'être pressé (pause). */
    [[nodiscard]] bool pauseTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton X vient d'être pressé (reset). */
    [[nodiscard]] bool resetPressed() noexcept;

    /* Vrai une seule fois, au moment où la combinaison LB + RB vient d'être pressée
     * (quitter). Une combinaison évite les sorties accidentelles. */
    [[nodiscard]] bool quitPressed() noexcept;

    /* Vrai une seule fois, au moment où le bouton A vient d'être pressé (bascule la
     * livrée Gendarmerie, comme la touche L). */
    [[nodiscard]] bool liveryTogglePressed() noexcept;

    /* Vrai une seule fois, au moment où la croix directionnelle haut vient d'être
     * pressée (bascule le mode assisté, comme la touche M). */
    [[nodiscard]] bool assistTogglePressed() noexcept;

    /* Remet le levier de collectif à zéro. */
    void reset() noexcept { m_collective = 0.0f; }

private:
    float m_collective = 0.0f;  /* position mémorisée du levier de collectif */
    bool  m_prevY      = false; /* état du bouton Y à l'image précédente */
    bool  m_prevStart  = false; /* état du bouton Start à l'image précédente */
    bool  m_prevB      = false; /* état du bouton B à l'image précédente */
    bool  m_prevBack   = false; /* état du bouton Back à l'image précédente */
    bool  m_prevX      = false; /* état du bouton X à l'image précédente */
    bool  m_prevQuit   = false; /* état de la combinaison LB + RB à l'image précédente */
    bool  m_prevA      = false; /* état du bouton A à l'image précédente */
    bool  m_prevDpadUp = false; /* état de la croix haut à l'image précédente */
};

}  /* namespace artouste::input */
