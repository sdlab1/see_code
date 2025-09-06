// src/gui/renderer.h
#ifndef SEE_CODE_RENDERER_H
#define SEE_CODE_RENDERER_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdint.h> // Для uint32_t

typedef struct Renderer Renderer;

// --- Основные функции управления ---
Renderer* renderer_create(int width, int height);
void renderer_destroy(Renderer* renderer);
void renderer_resize(Renderer* renderer, int width, int height);

// --- Функции рендеринга кадра ---
void renderer_begin_frame(Renderer* renderer);
void renderer_flush(Renderer* renderer);
int renderer_end_frame(Renderer* renderer);

// --- Функции отрисовки (добавляют данные в батч) ---
void renderer_clear(Renderer* renderer, float r, float g, float b, float a);
void renderer_draw_quad(Renderer* renderer, float x, float y, float width, float height, uint32_t color);
void renderer_draw_textured_quad(Renderer* renderer, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color);
// --- ИЗМЕНЕНИЕ: Добавлен параметр max_width для обрезки текста ---
void renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, uint32_t color, float max_width);

// --- Геттеры ---
int renderer_get_width(const Renderer* renderer);
int renderer_get_height(const Renderer* renderer);
GLuint renderer_get_font_atlas_texture(const Renderer* renderer);

#endif // SEE_CODE_RENDERER_H
