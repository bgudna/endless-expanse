#include "render/Shader.h"

#include "core/Log.h"

#include <string>

namespace ee {

namespace {
GLuint compile(GLenum type, const char* name, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        EE_LOG_ERROR("Shader '%s' (%s) failed to compile:\n%s", name,
                     type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
} // namespace

bool ShaderProgram::build(const char* name, const char* vertexSrc, const char* fragmentSrc) {
    destroy();
    GLuint vs = compile(GL_VERTEX_SHADER, name, vertexSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, name, fragmentSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program_, sizeof log, nullptr, log);
        EE_LOG_ERROR("Shader program '%s' failed to link:\n%s", name, log);
        destroy();
        return false;
    }
    return true;
}

void ShaderProgram::destroy() {
    if (program_) glDeleteProgram(program_);
    program_ = 0;
}

} // namespace ee
