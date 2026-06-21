/*
 * Turbine.hpp
 * Turbine Artouste de l'Alouette II, modélisée comme une petite machine à états.
 * Le démarrage se fait en trois temps :
 *   1. la turbine (générateur) monte seule en régime ; le rotor reste immobile,
 *      les pales ne tournent pas ;
 *   2. la turbine tient son plein régime quelques secondes, frein rotor encore
 *      serré : c'est le temps que prend le pilote avant de lâcher le frein ;
 *   3. le frein lâché, le rotor s'engage par la roue libre (celle qui permet
 *      aussi l'autorotation) et les pales accélèrent jusqu'au régime de vol.
 * Le lâcher du frein lui-même n'est pas une commande du joueur : il est simulé
 * par le délai de l'étape 2.
 * On suit donc deux régimes distincts, chacun dans [0, 1] :
 *   - le régime turbine, qui pilote le son (sifflement de montée en régime) ;
 *   - le régime rotor, qui pilote la portance (le modèle de vol le multiplie à
 *     la poussée et à l'anti-couple) et l'animation des pales.
 * Tant que la turbine n'est pas lancée, le rotor reste à l'arrêt : pas de
 * portance, pas de pales qui tournent.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include "physics/constants.hpp"

namespace artouste::physics {

class Turbine {
public:
    /* Les états du cycle. Le démarrage passe par Demarrage (la turbine monte),
     * Attente (turbine au régime, frein rotor encore serré) puis Embrayage (le
     * rotor s'accélère) avant d'atteindre le plein régime. */
    enum class State { Arret, Demarrage, Attente, Embrayage, Regime, Extinction };

    /* Lance le démarrage si la turbine est coupée, sinon entame l'extinction.
     * Pendant une transition, bascule simplement vers l'autre sens. */
    void toggle() noexcept;

    /* Avance d'un pas de temps : fait évoluer les régimes selon l'état courant.
     * Le collectif [0, 1] sert à la charge thermique de la tuyère (plus on tire de
     * puissance, plus elle chauffe). */
    void update(float dt, float collective) noexcept;

    /* Met directement la turbine et le rotor en régime établi (100 %). Utile pour
     * les tests et pour un éventuel démarrage immédiat. */
    void forceRunning() noexcept;

    /* Régime du rotor [0, 1] : portance et rotation des pales. */
    [[nodiscard]] float rotorFraction() const noexcept { return m_rotor; }

    /* Régime de la turbine [0, 1] : sert au son (sifflement de la turbine). */
    [[nodiscard]] float turbineFraction() const noexcept { return m_turbine; }

    /* Température de la tuyère (gaz d'échappement, T4), en degrés Celsius. */
    [[nodiscard]] float exhaustTempC() const noexcept { return m_exhaustC; }

    [[nodiscard]] State state() const noexcept { return m_state; }

    /* Rotor entraîné (au moins en partie) : permet de savoir si un décollage est
     * possible. */
    [[nodiscard]] bool turning() const noexcept { return m_rotor > 0.0f; }

    /* Libellé court pour le HUD, en français. */
    [[nodiscard]] const char* label() const noexcept;

private:
    State m_state   = State::Arret;
    float m_turbine = 0.0f;  /* régime turbine [0, 1] */
    float m_rotor   = 0.0f;  /* régime rotor   [0, 1] */
    float m_brakeTimer = 0.0f;  /* s écoulées en Attente, frein rotor serré */
    float m_exhaustC = EXHAUST_TEMP_AMBIENT_C;  /* température tuyère, degrés Celsius */
};

}  /* namespace artouste::physics */
