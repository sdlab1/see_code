// src/gui/renderer/freetype_text_renderer_draw.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // Для COLOR_TEXT_DEFAULT, если нужно
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <math.h> // Для floorf

// Предполагаем, что эта структура определена в text_renderer.c или приватном заголовке
// struct TextRendererInternalData {
//     int is_freetype_initialized;
//     FT_Library ft_library;
//     FT_Face ft_face;
//     GLuint texture_atlas_id;
//     unsigned char* texture_atlas_data;
//     int atlas_width;
//     int atlas_height;
//     struct {
//         int is_loaded;
//         int width, height;
//         int bearing_x, bearing_y;
//         int advance;
//         int atlas_x, atlas_y; // Позиция в атласе
//     } glyph_cache[128]; // ASCII 0-127
//     GLuint shader_program_textured;
//     GLuint vbo_textured;
// };

// Вспомогательная функция для получения или загрузки глифа
// (Реализация находится в freetype_text_renderer_atlas.c)
int text_renderer_internal_load_glyph(struct TextRendererInternalData* tr_data, unsigned long char_code);

void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text) {
        return;
    }

    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;
    if (!tr_data) {
        log_error("TextRendererInternalData is NULL in text_renderer_draw_text");
        return;
    }

    if (!tr_data->is_freetype_initialized || !tr_data->shader_program_textured) {
        log_warn("FreeType not initialized or shader not ready, using placeholder for text: %.20s", text);
        float text_width = strlen(text) * 8.0f * scale;
        float text_height = 16.0f * scale;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        renderer_draw_quad(renderer, x, y, text_width, text_height, r, g, b, a);
        return;
    }

    // Извлечение компонентов цвета (RGBA)
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;

    // Матрица MVP (ортографическая проекция)
    float mvp[16];
    float w = (float)renderer_get_width(renderer);
    float h = (float)renderer_get_height(renderer);
    if (w <= 0 || h <= 0) {
        log_error("Invalid renderer dimensions for MVP matrix");
        return;
    }

    mvp[0] = 2.0f / w; mvp[1] = 0; mvp[2] = 0; mvp[3] = 0;
    mvp[4] = 0; mvp[5] = -2.0f / h; mvp[6] = 0; mvp[7] = 0;
    mvp[8] = 0; mvp[9] = 0; mvp[10] = 1; mvp[11] = 0;
    mvp[12] = -1; mvp[13] = 1; mvp[14] = 0; mvp[15] = 1;


    float cursor_x = x;
    float cursor_y = y;

    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = text[i];
        if (c > 127) {
             // Пропустить или обработать не-ASCII символы
             log_debug("Skipping non-ASCII character: %c (code %d)", c, c);
             continue;
        }

        // Загрузить глиф, если он еще не в кэше
        if (!text_renderer_internal_load_glyph(tr_data, c)) {
            log_warn("Failed to load glyph for character '%c'", c);
            continue; // Пропустить символ, если не удалось загрузить
        }

        struct glyph_cache_entry *glyph = &tr_data->glyph_cache[c];

        if (!glyph->is_loaded) {
             log_warn("Glyph for character '%c' marked as not loaded after load attempt", c);
             continue;
        }

        float x_pos = cursor_x + glyph->bearing_x * scale;
        // FreeType отсчитывает Y от верхнего края, OpenGL от нижнего.
        // y позиция - это позиция базовой линии. Нам нужно позиционировать текстуру так,
        // чтобы её верхний край был на (y - bearingY).
        float y_pos = cursor_y - (glyph->height - glyph->bearing_y) * scale;

        float w = glyph->width * scale;
        float h = glyph->height * scale;

        // UV координаты в атласе (нормализованные)
        float u0 = (float)glyph->atlas_x / (float)tr_data->atlas_width;
        float v0 = (float)glyph->atlas_y / (float)tr_data->atlas_height;
        float u1 = (float)(glyph->atlas_x + glyph->width) / (float)tr_data->atlas_width;
        float v1 = (float)(glyph->atlas_y + glyph->height) / (float)tr_data->atlas_height;

        // Рисуем текстурированный квадрат
        gl_primitives_draw_textured_quad(
            tr_data->shader_program_textured,
            x_pos, y_pos, w, h,
            u0, v0, u1, v1,
            r, g, b, a,
            mvp,
            tr_data->vbo_textured,
            tr_data->texture_atlas_id
        );

        // Перемещаем курсор
        cursor_x += (glyph->advance >> 6) * scale; // advance в 1/64 пикселях
    }
}
