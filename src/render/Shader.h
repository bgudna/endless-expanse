#pragma once

#include "render/gl.h"

namespace ee {

// Minimal GLSL program wrapper.
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram() { destroy(); }
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    bool build(const char* name, const char* vertexSrc, const char* fragmentSrc);
    void destroy();
    void use() const { glUseProgram(program_); }
    GLint uniform(const char* name) const { return glGetUniformLocation(program_, name); }

private:
    GLuint program_ = 0;
};

} // namespace ee
