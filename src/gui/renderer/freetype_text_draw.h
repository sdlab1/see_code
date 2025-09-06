// src/gui/renderer/freetype_text_draw.h
#ifndef SEE_CODE_FREETYPE_TEXT_DRAW_H
#define SEE_CODE_FREETYPE_TEXT_DRAW_H

#include <GLES2/gl2.h> // Для GLuint и т.д.
#include <stddef.h>    // Для size_t

// Forward declaration
typedef struct Renderer Renderer; // From parent renderer.h

/**
 * @brief Renders a UTF-8 encoded string at a specified position using FreeType.
 *
 * This function assumes FreeType is initialized and a shader program is available.
 * It loads glyphs into the texture atlas and draws them as textured quads.
 *
 * @param renderer The main renderer instance.
 * @param text The null-terminated UTF-8 string to render.
 * @param x The X screen coordinate (pixels from left).
 * @param y The Y screen coordinate (pixels from top).
 * @param scale A scaling factor for the text size.
 * @param color The color in 0xAARRGGBB format.
 */
void freetype_text_draw(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color);

#endif // SEE_CODE_FREETYPE_TEXT_DRAW_H
