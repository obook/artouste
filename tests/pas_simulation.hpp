/*
 * pas_simulation.hpp
 * Pas de simulation commun à toutes les suites de tests physiques.
 *
 * Une seule définition : deux suites qui simuleraient à des pas différents
 * mesureraient deux physiques différentes sans que rien ne le signale.
 *
 * Auteur : O. Booklage
 * Date : août 2026
 * Licence : GPL v2
 */

#pragma once

namespace essais {

constexpr float SIM_DT = 1.0f / 240.0f;

} /* namespace essais */
