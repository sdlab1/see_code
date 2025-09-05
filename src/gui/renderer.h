// src/gui/renderer.h
#ifndef SEE_CODE_RENDERER_H
#define SEE_CODE_RENDERER_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
// --- Добавлено для FreeType ---
#include <ft2build.h>
#include FT_FREETYPE_H
// --- Конец добавления ---

// Forward declaration
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
// --- ОБНОВЛЕННАЯ ФУНКЦИЯ РЕНДЕРИНГА ТЕКСТА ---
void renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color);
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ РЕНДЕРИНГА ТЕКСТА ---

// --- Добавлено для FreeType ---
// Инициализирует FreeType и загружает шрифт
int renderer_init_freetype(Renderer* renderer, const char* font_path);
// --- Конец добавления ---

#endif // SEE_CODE_RENDERER_H
