// src/gui/renderer/freetype_text_renderer_draw.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include "see_code/gui/renderer/gl_primitives.h" // Для gl_primitives_draw_textured_quad
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

// Предполагаем, что renderer_get_width и renderer_get_height определены в renderer.h или renderer.c
// extern int renderer_get_width(Renderer* renderer);
// extern int renderer_get_height(Renderer* renderer);

// Предполагаем, что эта структура определена в text_renderer.c или приватном заголовке
// struct glyph_cache_entry {
//     int is_loaded;
//     float u0, v0, u1, v1; // UV координаты в атласе
//     int width, height;
//     int bearing_x, bearing_y;
//     int advance_x; // В пикселях
// };
// struct TextRendererInternalData {
//     int is_freetype_initialized;
//     FT_Library ft_library;
//     FT_Face ft_face;
//     GLuint texture_atlas_id;
//     unsigned char* texture_atlas_data;
//     int atlas_width;
//     int atlas_height;
//     struct glyph_cache_entry glyph_cache[96]; // Для символов ASCII 32-126
//     GLuint shader_program_textured;
//     GLuint vbo_textured;
// };

// Предполагаем, что эта функция определена в freetype_text_renderer_atlas.c
extern int load_glyph_into_atlas_internal(struct TextRendererInternalData* tr_data, unsigned long char_code);

void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text) {
        log_warn("Invalid arguments to text_renderer_draw_text");
        return;
    }

    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;
    if (!tr_data) {
        log_error("TextRendererInternalData is NULL in text_renderer_draw_text");
        // Рисуем placeholder, если данные рендерера текста отсутствуют
        float text_width = strlen(text) * 8.0f * scale;
        float text_height = 16.0f * scale;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        renderer_draw_quad(renderer, x, y, text_width, text_height, r, g, b, a);
        return;
    }

    if (!tr_data->is_freetype_initialized || !tr_data->shader_program_textured) {
        log_warn("FreeType not initialized or textured shader not ready, using placeholder for text: %.20s", text);
        float text_width = strlen(text) * 8.0f * scale;
        float text_height = 16.0f * scale;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        renderer_draw_quad(renderer, x, y, text_width, text_height, r, g, b, a);
        return;
    }

    // Извлечение компонентов цвета (RGBA -> OpenGL float)
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

    // Ортографическая проекция: [0, w] x [0, h] -> [-1, 1] x [-1, 1]
    mvp[0] = 2.0f / w; mvp[1] = 0; mvp[2] = 0; mvp[3] = 0;
    mvp[4] = 0; mvp[5] = -2.0f / h; mvp[6] = 0; mvp[7] = 0;
    mvp[8] = 0; mvp[9] = 0; mvp[10] = 1; mvp[11] = 0;
    mvp[12] = -1; mvp[13] = 1; mvp[14] = 0; mvp[15] = 1;


    float cursor_x = x;
    float cursor_y = y; // Базовая линия

    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = text[i];
        // Обрабатываем только печатаемые ASCII символы (32-126)
        if (c < 32 || c > 126) {
             log_debug("Skipping non-printable or non-ASCII character: %c (code %d)", c, c);
             continue;
        }

        // Загрузить глиф, если он еще не в кэше
        if (!load_glyph_into_atlas_internal(tr_data, c)) {
            log_warn("Failed to load glyph for character '%c' (U+%04X)", c, c);
            continue; // Пропустить символ, если не удалось загрузить
        }

        // Индекс в кэше (предполагаем, что кэш для 32-126)
        int cache_index = c - 32;
        if (cache_index < 0 || cache_index >= 96 || !tr_data->glyph_cache[cache_index].is_loaded) {
             log_warn("Glyph for character '%c' not found in cache after load attempt", c);
             continue;
        }

        struct glyph_cache_entry *glyph = &tr_data->glyph_cache[cache_index];

        // Позиция квадрата для рендеринга глифа
        float x_pos = cursor_x + glyph->bearing_x * scale;
        // OpenGL Y растет вверх, FreeType Y растет вниз.
        // y_pos - позиция нижнего левого угла текстуры.
        float y_pos = cursor_y - (glyph->height - glyph->bearing_y) * scale;

        float w = glyph->width * scale;
        float h = glyph->height * scale;

        // UV координаты из кэша
        float u0 = glyph->u0;
        float v0 = glyph->v0;
        float u1 = glyph->u1;
        float v1 = glyph->v1;

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
        cursor_x += glyph->advance_x * scale;
    }
}
