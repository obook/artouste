// Alias et helpers mathématiques. Centralise GLM derrière des noms courts
// pour éviter de polluer les sites d'appel avec glm::vec3, etc.
//
// Convention du projet :
//   - repère monde Y-up, X est, Z sud
//   - repère corps X avant, Y haut, Z droite
//   - quaternions Hamilton (convention GLM)
//   - unités SI partout : mètres, secondes, kilogrammes, radians.

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

// Conversions courtes
[[nodiscard]] inline constexpr float deg2rad(float deg) noexcept {
    return deg * DEG_TO_RAD;
}
[[nodiscard]] inline constexpr float rad2deg(float rad) noexcept {
    return rad * RAD_TO_DEG;
}

// Saturations utiles
template <typename T>
[[nodiscard]] inline constexpr T clamp(T value, T lo, T hi) noexcept {
    return std::clamp(value, lo, hi);
}

[[nodiscard]] inline constexpr float saturate(float value) noexcept {
    return clamp(value, 0.0f, 1.0f);
}

// Interpolation linéaire scalaire/vectorielle
template <typename T>
[[nodiscard]] inline T lerp(const T& a, const T& b, float t) noexcept {
    return a + (b - a) * t;
}

// Filtre passe-bas du premier ordre. tau est la constante de temps en secondes.
// Renvoie la nouvelle valeur lissée pour un pas dt. tau = 0 => sortie = cible.
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

// Interpolation d'angle qui prend toujours le plus court chemin sur le cercle.
// Utile pour la caméra chase qui suit le cap avec un retard.
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

}  // namespace artouste
