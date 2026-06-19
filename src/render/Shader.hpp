// Programme OpenGL minimal : compile un couple vertex/fragment, expose des
// setters d'uniformes typés. RAII sur le GLuint pour éviter les fuites.

#pragma once

#include "util/Math.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace artouste::render {

class Shader {
public:
    Shader() = default;

    // Charge et compile depuis des fichiers GLSL séparés.
    // Lance std::runtime_error en cas d'échec de compilation/édition de liens.
    Shader(const std::filesystem::path& vertex_path,
           const std::filesystem::path& fragment_path);

    ~Shader();

    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void use() const;

    // Uniformes. Pas d'assertions ici : un nom inconnu est juste ignoré
    // par OpenGL (location == -1).
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

}  // namespace artouste::render
