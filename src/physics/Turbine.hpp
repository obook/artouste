/*
 * Turbine.hpp
 * Turbine Artouste de l'Alouette II, modélisée comme une petite machine à états.
 * Le démarrage se fait en deux temps, comme sur une vraie turbine libre :
 *   1. la turbine (générateur) monte seule en régime ; le rotor reste immobile,
 *      retenu par le frein rotor, donc les pales ne tournent pas ;
 *   2. le frein relâché, la turbine libre entraîne le rotor par la roue libre
 *      (la même qui permet l'autorotation en cas de panne) ; les pales accélèrent
 *      jusqu'au régime de vol.
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

namespace artouste::physics {

class Turbine {
public:
    /* Les états du cycle. Le démarrage passe par Demarrage (la turbine monte)
     * puis Embrayage (le rotor s'accélère) avant d'atteindre le plein régime. */
    enum class State { Arret, Demarrage, Embrayage, Regime, Extinction };

    /* Lance le démarrage si la turbine est coupée, sinon entame l'extinction.
     * Pendant une transition, bascule simplement vers l'autre sens. */
    void toggle() noexcept;

    /* Avance d'un pas de temps : fait évoluer les régimes selon l'état courant. */
    void update(float dt) noexcept;

    /* Met directement la turbine et le rotor en régime établi (100 %). Utile pour
     * les tests et pour un éventuel démarrage immédiat. */
    void forceRunning() noexcept;

    /* Régime du rotor [0, 1] : portance et rotation des pales. */
    [[nodiscard]] float rotorFraction() const noexcept { return m_rotor; }

    /* Régime de la turbine [0, 1] : sert au son (sifflement de la turbine). */
    [[nodiscard]] float turbineFraction() const noexcept { return m_turbine; }

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
};

}  /* namespace artouste::physics */
