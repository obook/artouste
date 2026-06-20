/*
 * Math.hpp
 * Alias et petites fonctions mathématiques. On regroupe ici la bibliothèque
 * GLM derrière des noms courts (vec3, mat4...) pour alléger le reste du code.
 *
 * Conventions du projet :
 *   - repère monde : Y vers le haut, X vers l'est, Z vers le sud
 *   - repère corps : X vers l'avant, Y vers le haut, Z vers la droite
 *   - quaternions de Hamilton (convention GLM)
 *   - unités SI partout : mètres, secondes, kilogrammes, radians.
 *
 * Auteur : O. Booklage
 * Date : juin 2026
 * Licence : GPL v2
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

namespace artouste {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using quat = glm::quat;

inline constexpr float PI         = 3.14159265358979323846f;
inline constexpr float TWO_PI     = 2.0f * PI;
inline constexpr float HALF_PI    = 0.5f * PI;
inline constexpr float DEG_TO_RAD = PI / 180.0f;
inline constexpr float RAD_TO_DEG = 180.0f / PI;

/* Conversions courtes entre degrés et radians */
[[nodiscard]] inline constexpr float deg2rad(float deg) noexcept {
    return deg * DEG_TO_RAD;
}
[[nodiscard]] inline constexpr float rad2deg(float rad) noexcept {
    return rad * RAD_TO_DEG;
}

/* Bornage d'une valeur dans un intervalle [lo, hi] */
template <typename T>
[[nodiscard]] inline constexpr T clamp(T value, T lo, T hi) noexcept {
    return std::clamp(value, lo, hi);
}

[[nodiscard]] inline constexpr float saturate(float value) noexcept {
    return clamp(value, 0.0f, 1.0f);
}

/* Interpolation linéaire entre a et b, pour un scalaire ou un vecteur */
template <typename T>
[[nodiscard]] inline T lerp(const T& a, const T& b, float t) noexcept {
    return a + (b - a) * t;
}

/*
 * Filtre passe-bas du premier ordre : lisse une valeur qui tend vers une cible.
 * "tau" est la constante de temps en secondes (plus elle est grande, plus le
 * lissage est lent). On renvoie la valeur lissée après un pas "dt".
 * Cas particulier : tau = 0 => la sortie vaut directement la cible.
 */
[[nodiscard]] inline float lowPass(float current, float target, float dt, float tau) noexcept {
    if (tau <= 0.0f) {
        return target;
    }
    const float alpha = 1.0f - std::exp(-dt / tau);
    return current + alpha * (target - current);
}

[[nodiscard]] inline vec3 lowPass(const vec3& current, const vec3& target, float dt,
                                  float tau) noexcept {
    if (tau <= 0.0f) {
        return target;
    }
    const float alpha = 1.0f - std::exp(-dt / tau);
    return current + alpha * (target - current);
}

/*
 * Interpolation entre deux angles qui prend toujours le plus court chemin sur
 * le cercle. Utile pour la caméra de poursuite, qui suit le cap avec un retard.
 */
[[nodiscard]] inline float lerpAngle(float current, float target, float t) noexcept {
    float delta = std::fmod(target - current + PI, TWO_PI) - PI;
    if (delta < -PI) {
        delta += TWO_PI;
    }
    return current + delta * t;
}

[[nodiscard]] inline float lowPassAngle(float current, float target, float dt,
                                        float tau) noexcept {
    if (tau <= 0.0f) {
        return target;
    }
    const float alpha = 1.0f - std::exp(-dt / tau);
    return lerpAngle(current, target, alpha);
}

}  /* namespace artouste */
