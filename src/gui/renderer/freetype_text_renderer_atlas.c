// src/gui/renderer/freetype_text_renderer_atlas.c
#include "see_code/gui/renderer/text_renderer.h" // Предполагаем, что внутренняя структура объявлена тут или в приватном заголовке
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

// Предполагаем, что размеры определены где-то (например, в config.h или константы)
#define TEXTURE_ATLAS_WIDTH  1024
#define TEXTURE_ATLAS_HEIGHT 1024
#define GLYPH_PADDING 1 // Пиксели отступа между глифами в атласе

// Предполагаем, что эта структура определена в text_renderer.c или приватном заголовке
// struct TextRendererInternalData {
//     ...
//     struct glyph_cache_entry {
//         int is_loaded;
//         int width, height;
//         int bearing_x, bearing_y;
//         int advance;
//         int atlas_x, atlas_y;
//     } glyph_cache[128];
//     ...
// };

// Вспомогательная функция для поиска свободного места в атласе (очень простая реализация)
// Возвращает 1, если место найдено, и заполняет pen_x, pen_y
static int find_free_space_in_atlas(struct TextRendererInternalData* tr_data, int glyph_width, int glyph_height, int *out_x, int *out_y) {
    // Очень простой алгоритм "упаковки в линию"
    // Можно улучшить до полноценного алгоритма упаковки прямоугольников
    static int current_pen_x = 0;
    static int current_pen_y = 0;
    static int max_row_height = 0;

    // Проверим, помещается ли глиф в текущую строку
    if (current_pen_x + glyph_width + GLYPH_PADDING <= TEXTURE_ATLAS_WIDTH) {
        *out_x = current_pen_x;
        *out_y = current_pen_y;
        current_pen_x += glyph_width + GLYPH_PADDING;
        if (glyph_height + GLYPH_PADDING > max_row_height) {
            max_row_height = glyph_height + GLYPH_PADDING;
        }
        return 1;
    }

    // Не помещается, переходим на новую строку
    current_pen_x = 0;
    current_pen_y += max_row_height;
    max_row_height = 0;

    // Проверим, помещается ли теперь
    if (current_pen_y + glyph_height + GLYPH_PADDING <= TEXTURE_ATLAS_HEIGHT &&
        current_pen_x + glyph_width + GLYPH_PADDING <= TEXTURE_ATLAS_WIDTH) {
        *out_x = current_pen_x;
        *out_y = current_pen_y;
        current_pen_x += glyph_width + GLYPH_PADDING;
        max_row_height = glyph_height + GLYPH_PADDING;
        return 1;
    }

    // Атлас заполнен
    log_error("Texture atlas is full, cannot load more glyphs");
    return 0;
}

// Функция для обновления атласа текстур OpenGL
static void update_texture_atlas(struct TextRendererInternalData* tr_data) {
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tr_data->atlas_width, tr_data->atlas_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);
    // Предполагаем, что параметры текстуры уже установлены при инициализации
    glBindTexture(GL_TEXTURE_2D, 0);
}


int text_renderer_internal_load_glyph(struct TextRendererInternalData* tr_data, unsigned long char_code) {
    if (!tr_data || !tr_data->is_freetype_initialized || char_code > 127) {
        return 0;
    }

    if (tr_data->glyph_cache[char_code].is_loaded) {
        return 1; // Уже загружен
    }

    // Загружаем глиф в FreeType
    if (FT_Load_Char(tr_data->ft_face, char_code, FT_LOAD_RENDER)) {
        log_error("Failed to load glyph for character code %lu", char_code);
        return 0;
    }

    FT_GlyphSlot slot = tr_data->ft_face->glyph;
    int glyph_width = slot->bitmap.width;
    int glyph_height = slot->bitmap.rows;

    if (glyph_width == 0 || glyph_height == 0) {
        // Пробельные символы
        tr_data->glyph_cache[char_code].is_loaded = 1;
        tr_data->glyph_cache[char_code].width = glyph_width;
        tr_data->glyph_cache[char_code].height = glyph_height;
        tr_data->glyph_cache[char_code].bearing_x = slot->bitmap_left;
        tr_data->glyph_cache[char_code].bearing_y = slot->bitmap_top;
        tr_data->glyph_cache[char_code].advance = slot->advance.x;
        tr_data->glyph_cache[char_code].atlas_x = 0; // Не используется
        tr_data->glyph_cache[char_code].atlas_y = 0; // Не используется
        return 1;
    }

    // Найти место в атласе
    int atlas_x, atlas_y;
    if (!find_free_space_in_atlas(tr_data, glyph_width, glyph_height, &atlas_x, &atlas_y)) {
        return 0; // Нет места
    }

    // Копируем битмап глифа в атлас
    for (int y = 0; y < glyph_height; y++) {
        unsigned char *src_row = slot->bitmap.buffer + y * slot->bitmap.pitch;
        unsigned char *dst_row = tr_data->texture_atlas_data + (atlas_y + y) * tr_data->atlas_width + atlas_x;
        memcpy(dst_row, src_row, glyph_width);
    }

    // Обновить кэш
    tr_data->glyph_cache[char_code].is_loaded = 1;
    tr_data->glyph_cache[char_code].width = glyph_width;
    tr_data->glyph_cache[char_code].height = glyph_height;
    tr_data->glyph_cache[char_code].bearing_x = slot->bitmap_left;
    tr_data->glyph_cache[char_code].bearing_y = slot->bitmap_top;
    tr_data->glyph_cache[char_code].advance = slot->advance.x;
    tr_data->glyph_cache[char_code].atlas_x = atlas_x;
    tr_data->glyph_cache[char_code].atlas_y = atlas_y;

    // Обновить OpenGL текстуру
    update_texture_atlas(tr_data);

    log_debug("Loaded glyph U+%04lX ('%c') into atlas at (%d,%d)", char_code, (char)char_code, atlas_x, atlas_y);
    return 1;
}

// Инициализация атласа текстур
int text_renderer_init_atlas(struct TextRendererInternalData* tr_data) {
    if (!tr_data) return 0;

    tr_data->atlas_width = TEXTURE_ATLAS_WIDTH;
    tr_data->atlas_height = TEXTURE_ATLAS_HEIGHT;

    tr_data->texture_atlas_data = (unsigned char*)calloc(1, tr_data->atlas_width * tr_data->atlas_height);
    if (!tr_data->texture_atlas_data) {
        log_error("Failed to allocate memory for texture atlas data");
        return 0;
    }

    glGenTextures(1, &tr_data->texture_atlas_id);
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tr_data->atlas_width, tr_data->atlas_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    log_info("Texture atlas initialized (%dx%d)", tr_data->atlas_width, tr_data->atlas_height);
    return 1;
}

// Очистка атласа
void text_renderer_cleanup_atlas(struct TextRendererInternalData* tr_data) {
    if (!tr_data) return;
    if (tr_data->texture_atlas_id) {
        glDeleteTextures(1, &tr_data->texture_atlas_id);
        tr_data->texture_atlas_id = 0;
    }
    if (tr_data->texture_atlas_data) {
        free(tr_data->texture_atlas_data);
        tr_data->texture_atlas_data = NULL;
    }
    tr_data->atlas_width = 0;
    tr_data->atlas_height = 0;
    memset(tr_data->glyph_cache, 0, sizeof(tr_data->glyph_cache));
}
