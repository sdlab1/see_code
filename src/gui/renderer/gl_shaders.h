// src/gui/renderer/gl_shaders.h
#ifndef SEE_CODE_GL_SHADERS_H
#define SEE_CODE_GL_SHADERS_H

#include <GLES2/gl2.h>

/**
 * @brief Compiles a shader object from source code.
 *
 * @param type The type of shader (e.g., GL_VERTEX_SHADER, GL_FRAGMENT_SHADER).
 * @param source A null-terminated string containing the GLSL source code.
 * @return The ID of the compiled shader, or 0 on failure.
 */
GLuint gl_shaders_compile_shader(GLenum type, const char* source);

/**
 * @brief Links two compiled shaders into a program object.
 *
 * @param vertex_shader The ID of a compiled vertex shader.
 * @param fragment_shader The ID of a compiled fragment shader.
 * @return The ID of the linked program, or 0 on failure.
 */
GLuint gl_shaders_create_program(GLuint vertex_shader, GLuint fragment_shader);

/**
 * @brief A convenience function to compile and link a program from sources.
 *
 * This combines `gl_shaders_compile_shader` and `gl_shaders_create_program`.
 *
 * @param vertex_source A null-terminated string containing the vertex shader GLSL source.
 * @param fragment_source A null-terminated string containing the fragment shader GLSL source.
 * @return The ID of the linked program, or 0 on failure.
 */
GLuint gl_shaders_create_program_from_sources(const char* vertex_source, const char* fragment_source);

// --- Predefined shaders ---
extern const char* gl_shaders_textured_vertex_shader_source;
extern const char* gl_shaders_textured_fragment_shader_source;
extern const char* gl_shaders_solid_vertex_shader_source;
extern const char* gl_shaders_solid_fragment_shader_source;
// --- End predefined shaders ---

#endif // SEE_CODE_GL_SHADERS_H
