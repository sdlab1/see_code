// src/gui/renderer/gl_context.h
#ifndef SEE_CODE_GL_CONTEXT_H
#define SEE_CODE_GL_CONTEXT_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GLContext GLContext;

/**
 * @brief Creates and initializes an EGL context.
 *
 * This function sets up the EGL display, chooses a config, creates a surface
 * (placeholder, would be provided by Termux:GUI in reality), and creates/activates
 * an EGL context.
 *
 * @param width Initial window width.
 * @param height Initial window height.
 * @return A pointer to the initialized GLContext, or NULL on failure.
 */
GLContext* gl_context_create(int width, int height);

/**
 * @brief Destroys the EGL context and associated resources.
 *
 * @param ctx The GLContext to destroy. If NULL, the function does nothing.
 */
void gl_context_destroy(GLContext* ctx);

/**
 * @brief Begins a rendering frame by setting the viewport.
 *
 * @param ctx The GLContext.
 * @return 1 on success, 0 on failure.
 */
int gl_context_begin_frame(GLContext* ctx);

/**
 * @brief Ends a rendering frame by swapping buffers.
 *
 * @param ctx The GLContext.
 * @return 1 on success, 0 on failure.
 */
int gl_context_end_frame(GLContext* ctx);

/**
 * @brief Resizes the underlying surface/context viewport.
 *
 * @param ctx The GLContext.
 * @param width The new width.
 * @param height The new height.
 */
void gl_context_resize(GLContext* ctx, int width, int height);

/**
 * @brief Clears the framebuffer with a given color.
 *
 * @param r Red component (0.0 - 1.0).
 * @param g Green component (0.0 - 1.0).
 * @param b Blue component (0.0 - 1.0).
 * @param a Alpha component (0.0 - 1.0).
 */
void gl_context_clear(float r, float g, float b, float a);

// --- Геттеры для внутренних данных, необходимых другим модулям ---
EGLDisplay gl_context_get_display(const GLContext* ctx);
EGLSurface gl_context_get_surface(const GLContext* ctx);
EGLContext gl_context_get_context(const GLContext* ctx);
int gl_context_get_width(const GLContext* ctx);
int gl_context_get_height(const GLContext* ctx);
// --- Конец геттеров ---

#ifdef __cplusplus
}
#endif

#endif // SEE_CODE_GL_CONTEXT_H
