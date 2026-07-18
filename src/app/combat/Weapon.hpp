/*
 * Weapon.hpp
 * Lance-roquettes de l'appareil (mode zombie) : décide seulement QUAND un tir
 * part (cadence limitée, gâchette maintenue) et gère les munitions à recharge
 * automatique (le chargeur se remplit à nouveau dès qu'il est vide : munitions
 * de fait infinies, aucune manoeuvre de recharge à faire). La roquette
 * elle-même -- vol, explosion au sol, dégâts de zone -- est gérée par
 * RocketSystem ; Weapon ne connaît ni la horde ni la géométrie du tir.
 *
 * Auteur : O. Booklage
 * Date : juillet 2026
 * Licence : GPL v2
 */

#pragma once

namespace artouste::app {

class Weapon {
public:
    /* Roquettes par chargeur -- exposé pour le HUD. Vidé, il se recharge tout
       seul (voir update) : le compteur redescend à 0 puis repart au plein. */
    static constexpr int AMMO_MAX = 20;

    /* Résultat d'un pas de mise à jour : un tir est-il parti ? L'appelant
       (CombatMode) lance alors une roquette et déclenche le son, sur le même
       principe que le son de démarrage turbine (comparaison d'état d'une image
       à l'autre). */
    struct FireResult {
        bool fired = false;  /* une roquette est partie ce pas de temps */
    };

    /* Avance l'arme d'un pas de temps : si la gâchette est tenue, la cadence le
       permet et il reste des munitions, décrémente le chargeur et signale un
       tir. Chargeur vide : recharge automatique après une courte pause, aucun
       tir en attendant. */
    FireResult update(float dt, bool triggerHeld) noexcept;

    [[nodiscard]] int  ammo() const noexcept { return m_ammo; }
    [[nodiscard]] bool reloading() const noexcept { return m_reloadTimer > 0.0f; }

private:
    float m_fireCooldownS = 0.0f;  /* s restantes avant le prochain tir possible */
    float m_reloadTimer   = 0.0f;  /* s restantes de recharge (0 = pas en recharge) */
    int   m_ammo          = AMMO_MAX;
};

}  /* namespace artouste::app */
