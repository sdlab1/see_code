// src/gui/renderer/freetype_text_draw.c
#include "see_code/gui/renderer/freetype_text_draw.h"
#include "see_code/gui/renderer/text_renderer.h" // Для доступа к внутренним данным TextRendererInternalData
#include "see_code/gui/renderer/gl_primitives.h" // Для рисования текстурированных квадратов
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h> // Для snprintf
#include <math.h>  // Для floorf

// --- Реализация freetype_text_draw ---
void freetype_text_draw(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text) {
        return;
    }

    // Получаем указатель на внутренние данные text_renderer
    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;
    if (!tr_data || !tr_data->is_freetype_initialized || !tr_data->shader_program_textured) {
        log_error("FreeType not initialized or shader program not available in freetype_text_draw");
        return;
    }

    log_debug("FreeType is initialized, rendering text: %.50s", text ? text : "(null)");

    float cursor_x = x;
    float cursor_y = y; // Базовая линия

    // Матрица MVP (предполагаем ортографическую проекцию)
    float mvp[16] = {
         2.0f / renderer_get_width(renderer), 0, 0, 0,
         0, -2.0f / renderer_get_height(renderer), 0, 0,
         0, 0, 1, 0,
        -1, 1, 0, 1
    };

    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = text[i];
        if (c < 32 || c > 126) continue; // Только ASCII для простоты

        // Загружаем глиф в атлас (если еще не загружен)
        if (!load_glyph_into_atlas_internal(tr_data, c)) {
            log_warn("Failed to load glyph U+%04X for character '%c'", c, c);
            continue;
        }

        // Ищем загруженный глиф в кэше
        int cache_index = c - 32; // ASCII 32-126 -> индексы 0-95
        if (cache_index < 0 || cache_index >= 96 || !tr_data->glyph_cache[cache_index].is_loaded) {
            log_warn("Glyph U+%04X for character '%c' not found in cache after loading", c, c);
            continue;
        }

        struct { unsigned long uc; float u0,v0,u1,v1; int w,h,bx,by,ax; int loaded; }* cached_glyph = (void*)&tr_data->glyph_cache[cache_index];

        float x_pos = cursor_x + cached_glyph->bx * scale;
        // В OpenGL Y растет вверх, а у нас вниз. Корректируем.
        float y_pos = cursor_y - (cached_glyph->h - cached_glyph->by) * scale;
        float w = cached_glyph->w * scale;
        float h = cached_glyph->h * scale;

        // Рисуем текстурированный квадрат
        // Используем gl_primitives для рисования
        if (!gl_primitives_draw_textured_quad(
            tr_data->shader_program_textured,
            tr_data->texture_atlas_id,
            x_pos, y_pos, w, h,
            cached_glyph->u0, cached_glyph->v0, cached_glyph->u1, cached_glyph->v1,
            color, // Передаем цвет в функцию
            mvp,
            tr_data->vbo // Используем общий VBO
        )) {
            log_warn("Failed to draw textured quad for glyph U+%04X", c);
        }

        cursor_x += cached_glyph->ax * scale;
    }
    log_debug("Finished rendering text with FreeType");
}
// --- Конец реализации freetype_text_draw ---
