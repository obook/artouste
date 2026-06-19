#include "render/Shader.hpp"

#include <glad/glad.h>

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace artouste::render {

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("shader introuvable : " + path.string());
    }
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

unsigned int compile(unsigned int type, const std::string& source,
                     const std::filesystem::path& origin) {
    const GLuint shader = glCreateShader(type);
    const char*  c_str  = source.c_str();
    glShaderSource(shader, 1, &c_str, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        std::array<char, 2048> log{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glDeleteShader(shader);
        const char* stage = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        throw std::runtime_error(std::string("shader ") + stage + " (" + origin.string() +
                                 ") :\n" + log.data());
    }
    return shader;
}

}  // namespace

Shader::Shader(const std::filesystem::path& vertex_path,
               const std::filesystem::path& fragment_path) {
    const std::string vsrc = read_file(vertex_path);
    const std::string fsrc = read_file(fragment_path);

    const GLuint vs = compile(GL_VERTEX_SHADER, vsrc, vertex_path);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc, fragment_path);

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint status = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        std::array<char, 2048> log{};
        glGetProgramInfoLog(m_program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glDeleteProgram(m_program);
        m_program = 0;
        throw std::runtime_error(std::string("édition de liens shader :\n") + log.data());
    }
}

Shader::~Shader() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
    }
}

Shader::Shader(Shader&& other) noexcept
    : m_program(std::exchange(other.m_program, 0)),
      m_locations(std::move(other.m_locations)) {}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_program != 0) {
            glDeleteProgram(m_program);
        }
        m_program   = std::exchange(other.m_program, 0);
        m_locations = std::move(other.m_locations);
    }
    return *this;
}

void Shader::use() const {
    glUseProgram(m_program);
}

int Shader::getLocation(const std::string& name) {
    if (auto it = m_locations.find(name); it != m_locations.end()) {
        return it->second;
    }
    const int loc      = glGetUniformLocation(m_program, name.c_str());
    m_locations[name]  = loc;
    return loc;
}

void Shader::setMat4(const std::string& name, const mat4& value) {
    glUniformMatrix4fv(getLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, const vec3& value) {
    glUniform3fv(getLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec4(const std::string& name, const vec4& value) {
    glUniform4fv(getLocation(name), 1, glm::value_ptr(value));
}

void Shader::setFloat(const std::string& name, float value) {
    glUniform1f(getLocation(name), value);
}

void Shader::setInt(const std::string& name, int value) {
    glUniform1i(getLocation(name), value);
}

}  // namespace artouste::render
