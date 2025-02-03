#include "../include/ivy_gl.h"
#include "../include/ivy_log.h"

static uint32_t build_named_shader(const char *code, GLenum shaderType, const char *name);

uint32_t client::util::create_texture(uint32_t width, uint32_t height, uint32_t glInternalFormat, uint32_t glFilter){
    uint32_t texture_handle;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture_handle);
    glTextureParameteri(texture_handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture_handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (glFilter)
    {
        glTextureParameteri(texture_handle, GL_TEXTURE_MAG_FILTER, glFilter);
        glTextureParameteri(texture_handle, GL_TEXTURE_MIN_FILTER, glFilter);
    }
    glTextureStorage2D(texture_handle, 1, glInternalFormat, width, height);
    return texture_handle;
}

uint32_t client::util::build_pipeline(const char *vertexShaderCode, const char *fragmentShaderCode) {
    uint32_t vertShader = build_named_shader(vertexShaderCode, GL_VERTEX_SHADER, "N/A");
    uint32_t fragShader = build_named_shader(fragmentShaderCode, GL_FRAGMENT_SHADER, "N/A");

    uint32_t shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertShader);
    glAttachShader(shaderProgram, fragShader);
    glLinkProgram(shaderProgram);

    GLint isLinked = 0;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE) {
        char errorLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, errorLog);
        glDeleteProgram(shaderProgram);
        fatal("ERROR COMPILING SHADER: %s", errorLog);
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return shaderProgram;
}

uint32_t client::util::build_named_program(const char *shaderCode, GLenum shaderType, const char *name) {
    uint32_t shader = build_named_shader(shaderCode, shaderType, name);

    uint32_t shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, shader);
    glLinkProgram(shaderProgram);

    GLint isLinked = 0;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE) {
        char errorLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, errorLog);
        glDeleteProgram(shaderProgram);
        fatal("ERROR COMPILING SHADER: %s", errorLog);
    }

    glDeleteShader(shader);

    return shaderProgram;
}

static uint32_t build_named_shader(const char *code, GLenum shaderType, const char *name) {
    uint32_t shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, (const GLchar *const *) &code, NULL);
    glCompileShader(shader);

    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        char errorLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, errorLog);
        glDeleteShader(shader); // Don't leak the shader.
        fatal("ERROR COMPILING SHADER \"%s\": %s", name, errorLog);
    }

    return shader;
}