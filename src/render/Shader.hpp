/*
 * Shader.hpp
 * Programme graphique : assemble un shader de sommets et un shader de
 * fragments en un programme utilisable par la carte graphique, et offre
 * des fonctions simples pour lui envoyer des valeurs (uniformes). La
 * ressource OpenGL est libérée automatiquement (RAII).
 *
 * Auteur : O. Booklage
 * Licence : GPL v2
 */

#pragma once

#include "util/Math.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace artouste::render {

class Shader {
public:
    Shader() = default;

    /*
     * Charge et compile les deux shaders depuis des fichiers GLSL.
     * Lève std::runtime_error si la compilation ou l'assemblage échoue.
     */
    Shader(const std::filesystem::path& vertex_path,
           const std::filesystem::path& fragment_path);

    ~Shader();

    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void use() const;

    /*
     * Envoi de valeurs (uniformes) au programme. Un nom inconnu est
     * simplement ignoré par OpenGL, sans erreur.
     */
    void setMat4(const std::string& name, const mat4& value);
    void setVec3(const std::string& name, const vec3& value);
    void setVec4(const std::string& name, const vec4& value);
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);

    [[nodiscard]] unsigned int id() const noexcept { return m_program; }

private:
    int getLocation(const std::string& name);

    unsigned int                            m_program  = 0;
    std::unordered_map<std::string, int>    m_locations;
};

}  /* namespace artouste::render */
