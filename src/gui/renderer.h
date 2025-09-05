// src/gui/renderer.h
#ifndef SEE_CODE_RENDERER_H
#define SEE_CODE_RENDERER_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
// Remove freetype includes from here, they are now in text_renderer.h

// Forward declarations
typedef struct Renderer Renderer;

// Renderer functions
Renderer* renderer_create(int width, int height);
void renderer_destroy(Renderer* renderer);
int renderer_begin_frame(Renderer* renderer);
int renderer_end_frame(Renderer* renderer);
void renderer_resize(Renderer* renderer, int width, int height);

// Rendering functions
void renderer_clear(float r, float g, float b, float a);
void renderer_draw_quad(float x, float y, float width, float height,
                       float r, float g, float b, float a);

// --- ОБНОВЛЕНИЕ: Объявление новой функции ---
// Объявляем основную функцию рендеринга текста, реализация в text_renderer.c
void renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color);
// --- КОНЕЦ ОБНОВЛЕНИЯ ---

#endif // SEE_CODE_RENDERER_H
