// src/gui/renderer/text_renderer.h
#ifndef SEE_CODE_TEXT_RENDERER_H
#define SEE_CODE_TEXT_RENDERER_H

#include <GLES2/gl2.h> // Для GLuint и т.д.
#include <stddef.h>    // Для size_t

// Forward declaration
typedef struct Renderer Renderer; // From parent renderer.h

/**
 * @brief Initializes the text rendering subsystem.
 *
 * Attempts to initialize FreeType and load a suitable font.
 * If successful, text can be rendered using vector glyphs.
 * If FreeType fails, a fallback to simple quad rendering is used.
 * This function should be called once during Renderer initialization.
 *
 * @param renderer The main renderer instance.
 * @param font_path_hint A hint for the path to a TrueType/OpenType font file.
 *                       Can be NULL to use built-in defaults.
 * @return 1 on success (including successful fallback setup), 0 on failure.
 */
int text_renderer_init(Renderer* renderer, const char* font_path_hint);

/**
 * @brief Cleans up resources used by the text rendering subsystem.
 *
 * Frees FreeType library, faces, texture atlases, and associated data.
 * This function should be called during Renderer destruction.
 */
void text_renderer_cleanup(Renderer* renderer);

/**
 * @brief Renders a UTF-8 encoded string at a specified position.
 *
 * This is the primary function for drawing text. It uses FreeType if
 * initialized, otherwise falls back to drawing colored rectangles.
 *
 * @param renderer The main renderer instance.
 * @param text The null-terminated UTF-8 string to render.
 * @param x The X screen coordinate (pixels from left).
 * @param y The Y screen coordinate (pixels from top).
 * @param scale A scaling factor for the text size.
 * @param color The color in 0xAARRGGBB format.
 */
void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color);

#endif // SEE_CODE_TEXT_RENDERER_H
