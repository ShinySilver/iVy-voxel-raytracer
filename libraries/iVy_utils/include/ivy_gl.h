#pragma once

#include <cstdint>
#include "glad/gl.h"
#include "GLFW/glfw3.h"

namespace client::util {
    /**
     * @param width The width of the texture to create, in pixels
     * @param height The height of the texture to create, in pixels
     * @param glInternalFormat Internal format of the texture. See link below for valid formats.
     * @param glFilter Filtering function of the texture. Valid values: [NULL, GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR]
     * @return The id of the newly created texture.
     * @see To delete the texture after creation: destroy_texture(textureId)
     * @see For valid glInternalFormat values: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexStorage2D.xhtml
     */
    uint32_t create_texture(uint32_t width, uint32_t height, uint32_t glInternalFormat, uint32_t glFilter);

    /**
     * @param textureId The id of the texture to bind, as returned by create_texture
     * @param bindIndex The index of the texture in the current shader.
     * @param glUsage The expected usage of the texture. Driver-dependant. Valid values: [GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE]
     */
    #define bind_texture(textureId, bindIndex, glInternalFormat, glUsage)  \
        glBindImageTexture(bindIndex, textureId, 0, GL_FALSE, 0, glUsage, glInternalFormat)

    /**
     * @param textureId The id of the texture to destroy
     */
    #define destroy_texture(textureId) glDeleteTextures(1, &textureId)

    /**
     * @param vertexShaderCode The Code to the vertex shader glsl file
     * @param fragmentShaderCode The Code to the fragment shader glsl file
     * @return The id of the newly compiled rasterization pipeline.
     * @see destroy_program(programId)
     */
    uint32_t build_pipeline(const char *vertexShaderCode, const char *fragmentShaderCode);

    /**
     * @param computeShaderCode The Code to the compute shader glsl file
     * @param shaderType The GL shader type. Valid values: [GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER]
     * @return The id of the newly compiled shader.
     * @see destroy_program(programId)
     */
    uint32_t build_program(const char *shaderCode, GLenum shaderType);

    /**
     * @param programId The id of the program to destroy
     */
    #define destroy_program(programId) glDeleteProgram(programId)

    /**
     * @param shader_path the relative path of the glsl file to get text from
     */
    #define load_shader_code(shader_path) \

}