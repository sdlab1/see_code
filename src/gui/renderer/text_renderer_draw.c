// src/gui/renderer/text_renderer_draw.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h" // Для доступа к gl функциям и renderer_draw_quad
#include "see_code/gui/renderer/gl_context.h" // Для доступа к GLContext, если нужно
#include "see_code/gui/renderer/gl_shaders.h" // Для скомпилированных шейдеров
#include "see_code/gui/renderer/gl_primitives.h" // Для рисования квадратов
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // Для путей к шрифтам и констант
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h> // Для snprintf
#include <math.h>  // Для floorf

// Предполагаем, что struct TextRendererInternalData определена в text_renderer_core.c
// и что renderer->text_internal_data_private указывает на неё.
// Для этого примера, повторим определение структуры.
// В реальном проекте это лучше сделать через общий заголовочный файл или приватный заголовок.
struct TextRendererInternalData {
    int is_freetype_initialized;
    FT_Library ft_library;
    FT_Face ft_face;
    GLuint texture_atlas_id; // ID OpenGL текстуры для атласа глифов
    unsigned char* texture_atlas_data; // CPU-side данные атласа
    int atlas_width;
    int atlas_height;
    // Простой кэш глифов (ASCII 32-126)
    struct {
        unsigned long unicode_char;
        float u0, v0, u1, v1; // UV координаты в атласе
        int width, height;   // Размеры глифа
        int bearing_x, bearing_y; // Смещения от базовой линии
        int advance_x;         // Продвижение курсора
        int is_loaded;         // Флаг, загружен ли глиф
    } glyph_cache[96]; // ASCII 32 (space) до 126 (~)
    // Ресурсы OpenGL для рендеринга текста
    GLuint shader_program_textured; // Шейдерная программа для текстурированных квадратов
    GLuint vbo; // Общий VBO для текстовых квадратов
};

// --- Вспомогательная функция для загрузки глифа в атлас ---
static int load_glyph_into_atlas_internal(struct TextRendererInternalData* tr_data, unsigned long char_code) {
    if (!tr_data || !tr_data->is_freetype_initialized) {
        return 0;
    }

    // Проверяем, есть ли глиф уже в кэше
    int cache_index = char_code - 32; // ASCII 32-126 -> индексы 0-95
    if (cache_index < 0 || cache_index >= 96) {
        log_debug("Character U+%04lX is outside ASCII range 32-126, skipping", char_code);
        return 0; // Вне диапазона кэша
    }

    if (tr_data->glyph_cache[cache_index].is_loaded) {
        return 1; // Уже загружен
    }

    if (FT_Load_Char(tr_data->ft_face, char_code, FT_LOAD_RENDER)) {
        log_warn("Failed to load glyph for character U+%04lX", char_code);
        return 0;
    }

    FT_GlyphSlot slot = tr_data->ft_face->glyph;
    int bitmap_width = slot->bitmap.width;
    int bitmap_rows = slot->bitmap.rows;

    // Найдем место в атласе (простой алгоритм "первое подходящее место")
    // В реальной реализации лучше использовать более сложные аллокаторы
    int pen_x = 0, pen_y = 0;
    int row_height = 0;
    int found_space = 0;

    // Простой поиск места: идем по строкам
    for (int y = 0; y < tr_data->atlas_height - bitmap_rows; ) {
        int max_h_in_row = 0;
        for (int x = 0; x < tr_data->atlas_width - bitmap_width; ) {
            // Проверяем, свободно ли место в прямоугольнике (x,y,bitmap_width,bitmap_rows)
            int is_free = 1;
            for (int yy = y; yy < y + bitmap_rows && is_free; yy++) {
                for (int xx = x; xx < x + bitmap_width && is_free; xx++) {
                    if (tr_data->texture_atlas_data[yy * tr_data->atlas_width + xx] != 0) {
                        is_free = 0;
                    }
                }
            }
            if (is_free) {
                pen_x = x;
                pen_y = y;
                found_space = 1;
                max_h_in_row = bitmap_rows > max_h_in_row ? bitmap_rows : max_h_in_row;
                break; // Нашли место, выходим из внутреннего цикла
            } else {
                // Пропускаем занятую область
                x += bitmap_width + 1; // Простой сдвиг
            }
        }
        if (found_space) {
            row_height = max_h_in_row;
            break; // Нашли место, выходим из внешнего цикла
        }
        y += row_height + 1; // Переходим к следующей строке
        row_height = 0; // Сброс высоты для новой строки
    }

    if (!found_space) {
        log_warn("No space left in texture atlas for glyph U+%04lX", char_code);
        return 0; // Нет места
    }

    // Копируем битмап глифа в атлас
    for (int row = 0; row < bitmap_rows; row++) {
        for (int col = 0; col < bitmap_width; col++) {
            int atlas_index = (pen_y + row) * tr_data->atlas_width + (pen_x + col);
            int bitmap_index = row * slot->bitmap.width + col;
            // Используем альфа-канал
            tr_data->texture_atlas_data[atlas_index] = slot->bitmap.buffer[bitmap_index];
        }
    }

    // Обновляем текстуру OpenGL
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    // glTexSubImage2D более эффективен, если обновляется только часть
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tr_data->atlas_width, tr_data->atlas_height, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Добавляем глиф в кэш
    tr_data->glyph_cache[cache_index].unicode_char = char_code;
    tr_data->glyph_cache[cache_index].u0 = (float)pen_x / tr_data->atlas_width;
    tr_data->glyph_cache[cache_index].v0 = (float)pen_y / tr_data->atlas_height;
    tr_data->glyph_cache[cache_index].u1 = (float)(pen_x + bitmap_width) / tr_data->atlas_width;
    tr_data->glyph_cache[cache_index].v1 = (float)(pen_y + bitmap_rows) / tr_data->atlas_height;
    tr_data->glyph_cache[cache_index].width = bitmap_width;
    tr_data->glyph_cache[cache_index].height = bitmap_rows;
    tr_data->glyph_cache[cache_index].bearing_x = slot->bitmap_left;
    tr_data->glyph_cache[cache_index].bearing_y = slot->bitmap_top;
    tr_data->glyph_cache[cache_index].advance_x = slot->advance.x >> 6; // В пикселях
    tr_data->glyph_cache[cache_index].is_loaded = 1;

    log_debug("Loaded glyph U+%04lX into atlas at (%d,%d)", char_code, pen_x, pen_y);
    return 1;
}
// --- Конец вспомогательной функции ---

// --- ОСНОВНАЯ ФУНКЦИЯ РЕНДЕРИНГА ТЕКСТА ---
void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text) {
        return;
    }

    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;
    if (!tr_data) {
        log_error("TextRendererInternalData is NULL in text_renderer_draw_text");
        return;
    }

    if (tr_data->is_freetype_initialized && tr_data->shader_program_textured) {
        // --- FULL FREEType RENDERING ---
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

            if (!load_glyph_into_atlas_internal(tr_data, c)) continue;

            struct { unsigned long uc; float u0,v0,u1,v1; int w,h,bx,by,ax; int loaded; }* cached_glyph = NULL;
            int cache_index = c - 32;
            if (cache_index >= 0 && cache_index < 96 && tr_data->glyph_cache[cache_index].is_loaded) {
                // Приводим к временной структуре для удобства доступа
                cached_glyph = (void*)&tr_data->glyph_cache[cache_index];
            }
            if (!cached_glyph) continue;

            float x_pos = cursor_x + cached_glyph->bx * scale;
            // В OpenGL Y растет вверх, а у нас вниз. Корректируем.
            float y_pos = cursor_y - (cached_glyph->h - cached_glyph->by) * scale;
            float w = cached_glyph->w * scale;
            float h = cached_glyph->h * scale;

            // Рисуем текстурированный квадрат
            // Используем gl_primitives для рисования
            gl_primitives_draw_textured_quad(
                tr_data->shader_program_textured,
                tr_data->texture_atlas_id,
                x_pos, y_pos, w, h,
                cached_glyph->u0, cached_glyph->v0, cached_glyph->u1, cached_glyph->v1,
                color, // Передаем цвет в функцию
                mvp,
                tr_data->vbo // Используем общий VBO
            );

            cursor_x += cached_glyph->ax * scale;
        }
        // --- КОНЕЦ FULL FREEType RENDERING ---
    } else {
        // --- FALLBACK RENDERING ---
        log_debug("FreeType not initialized, using placeholder for text: %.20s", text);
        float text_width = strlen(text) * 8.0f * scale;
        float text_height = 16.0f * scale;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        // Предполагаем, что renderer_draw_quad определен в renderer.c и делегируется gl_primitives
        renderer_draw_quad(renderer, x, y, text_width, text_height, r, g, b, a);
        // --- КОНЕЦ FALLBACK RENDERING ---
    }
}
// --- КОНЕЦ ОСНОВНОЙ ФУНКЦИИ РЕНДЕРИНГА ТЕКСТА ---
