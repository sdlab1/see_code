// src/gui/renderer/gl_primitives.h
#ifndef SEE_CODE_GL_PRIMITIVES_H
#define SEE_CODE_GL_PRIMITIVES_H

#include <GLES2/gl2.h>

/**
 * @brief Draws a solid-colored quad using a provided shader program.
 *
 * This function assumes the shader program uses `position` and `color` attributes
 * and an `mvp` uniform matrix.
 *
 * @param program_id The ID of the compiled and linked shader program.
 * @param x The X coordinate of the top-left corner.
 * @param y The Y coordinate of the top-left corner.
 * @param width The width of the quad.
 * @param height The height of the quad.
 * @param r Red component (0.0 - 1.0).
 * @param g Green component (0.0 - 1.0).
 * @param b Blue component (0.0 - 1.0).
 * @param a Alpha component (0.0 - 1.0).
 * @param mvp A 4x4 float matrix (column-major order) for model-view-projection transformation.
 * @param vbo_id An existing VBO ID to use for uploading vertex data. If 0, a new one will be created (less efficient).
 * @return 1 on success, 0 on failure.
 */
int gl_primitives_draw_solid_quad(GLuint program_id, float x, float y, float width, float height,
                                 float r, float g, float b, float a,
                                 const float mvp[16], GLuint vbo_id);

/**
 * @brief Draws a textured quad using a provided shader program.
 *
 * This function assumes the shader program uses `position` and `texcoord` attributes,
 * an `mvp` uniform matrix, and a `tex` uniform sampler.
 *
 * @param program_id The ID of the compiled and linked shader program.
 * @param texture_id The ID of the OpenGL texture to apply.
 * @param x The X coordinate of the top-left corner.
 * @param y The Y coordinate of the top-left corner.
 * @param width The width of the quad.
 * @param height The height of the quad.
 * @param u0 The U texture coordinate of the top-left corner.
 * @param v0 The V texture coordinate of the top-left corner.
 * @param u1 The U texture coordinate of the bottom-right corner.
 * @param v1 The V texture coordinate of the bottom-right corner.
 * @param color_rgba The color tint/mask in 0xRRGGBBAA format. Alpha is used to modulate the texture alpha.
 * @param mvp A 4x4 float matrix (column-major order) for model-view-projection transformation.
 * @param vbo_id An existing VBO ID to use for uploading vertex data. If 0, a new one will be created (less efficient).
 * @return 1 on success, 0 on failure.
 */
int gl_primitives_draw_textured_quad(GLuint program_id, GLuint texture_id,
                                    float x, float y, float width, float height,
                                    float u0, float v0, float u1, float v1,
                                    unsigned int color_rgba,
                                    const float mvp[16], GLuint vbo_id);

#endif // SEE_CODE_GL_PRIMITIVES_H
