// src/gui/renderer/text_renderer.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h"
#include "see_code/gui/renderer/gl_primitives.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

// --- Внутренняя структура данных для text_renderer ---
// Объединяет всю информацию, необходимую для рендеринга текста
struct TextRendererInternalData {
    int is_freetype_initialized;
    FT_Library ft_library;
    FT_Face ft_face;
    
    // Текстурный атлас
    GLuint texture_atlas_id;
    unsigned char* texture_atlas_data;
    int atlas_width;
    int atlas_height;
    int atlas_pen_x; // Текущая позиция для вставки глифа
    int atlas_pen_y;
    int atlas_row_height;

    // Кэш глифов (для ASCII 32-126)
    struct {
        int is_loaded;
        float u0, v0, u1, v1;       // UV-координаты в атласе
        int width, height;          // Размеры глифа в пикселях
        int bearing_x, bearing_y;   // Смещение от базовой линии
        int advance_x;              // Насколько сдвинуть курсор после отрисовки
    } glyph_cache[96];

    // OpenGL ресурсы
    GLuint shader_program_textured;
    GLuint vbo;
};

// --- Внутренние статические функции ---

// Загружает глиф в атлас, если его там еще нет
static int load_glyph_into_atlas(struct TextRendererInternalData* tr_data, unsigned long char_code) {
    if (!tr_data || !tr_data->is_freetype_initialized || char_code < 32 || char_code > 126) {
        return 0;
    }

    int cache_index = char_code - 32;
    if (tr_data->glyph_cache[cache_index].is_loaded) {
        return 1; // Уже в кэше
    }

    if (FT_Load_Char(tr_data->ft_face, char_code, FT_LOAD_RENDER)) {
        log_warn("Failed to load glyph for character U+%04lX", char_code);
        return 0;
    }

    FT_GlyphSlot slot = tr_data->ft_face->glyph;
    int w = slot->bitmap.width;
    int h = slot->bitmap.rows;

    // Простой, но эффективный алгоритм упаковки атласа
    if (tr_data->atlas_pen_x + w + 1 > tr_data->atlas_width) {
        tr_data->atlas_pen_y += tr_data->atlas_row_height + 1;
        tr_data->atlas_pen_x = 0;
        tr_data->atlas_row_height = 0;
    }

    if (tr_data->atlas_pen_y + h + 1 > tr_data->atlas_height) {
        log_error("Texture atlas is full, cannot load more glyphs");
        return 0;
    }

    // Копируем битмап глифа в атлас
    for (int y = 0; y < h; y++) {
        void* dst = tr_data->texture_atlas_data + (tr_data->atlas_pen_y + y) * tr_data->atlas_width + tr_data->atlas_pen_x;
        void* src = slot->bitmap.buffer + y * slot->bitmap.pitch;
        memcpy(dst, src, w);
    }
    
    // Обновляем данные в кэше
    struct glyph_cache_entry* glyph = &tr_data->glyph_cache[cache_index];
    glyph->is_loaded = 1;
    glyph->u0 = (float)(tr_data->atlas_pen_x) / tr_data->atlas_width;
    glyph->v0 = (float)(tr_data->atlas_pen_y) / tr_data->atlas_height;
    glyph->u1 = (float)(tr_data->atlas_pen_x + w) / tr_data->atlas_width;
    glyph->v1 = (float)(tr_data->atlas_pen_y + h) / tr_data->atlas_height;
    glyph->width = w;
    glyph->height = h;
    glyph->bearing_x = slot->bitmap_left;
    glyph->bearing_y = slot->bitmap_top;
    glyph->advance_x = slot->advance.x >> 6;

    // Сдвигаем "каретку" атласа
    tr_data->atlas_pen_x += w + 1;
    if (h > tr_data->atlas_row_height) {
        tr_data->atlas_row_height = h;
    }

    // Обновляем всю текстуру (для простоты)
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tr_data->atlas_width, tr_data->atlas_height, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return 1;
}

// --- Публичные функции API ---

int text_renderer_init(Renderer* renderer, const char* font_path_hint) {
    if (!renderer) return 0;

    struct TextRendererInternalData* tr_data = calloc(1, sizeof(struct TextRendererInternalData));
    if (!tr_data) {
        log_error("Failed to allocate memory for TextRendererInternalData");
        return 0;
    }
    
    // 1. Инициализация FreeType
    if (FT_Init_FreeType(&tr_data->ft_library)) {
        log_error("Could not init FreeType library");
        free(tr_data);
        return 0;
    }

    // 2. Попытка загрузки шрифтов
    const char* fonts_to_try[] = {
        font_path_hint, FREETYPE_FONT_PATH, TRUETYPE_FONT_PATH, FALLBACK_FONT_PATH, NULL
    };
    for (int i = 0; fonts_to_try[i] != NULL; ++i) {
        if (FT_New_Face(tr_data->ft_library, fonts_to_try[i], 0, &tr_data->ft_face) == 0) {
            log_info("FreeType initialized with font: %s", fonts_to_try[i]);
            goto font_loaded;
        }
    }
    log_error("Failed to load any suitable font.");
    FT_Done_FreeType(tr_data->ft_library);
    free(tr_data);
    return 0;

font_loaded:
    FT_Set_Pixel_Sizes(tr_data->ft_face, 0, FONT_SIZE_DEFAULT);
    tr_data->is_freetype_initialized = 1;

    // 3. Инициализация атласа
    tr_data->atlas_width = ATLAS_WIDTH_DEFAULT;
    tr_data->atlas_height = ATLAS_HEIGHT_DEFAULT;
    tr_data->texture_atlas_data = calloc(1, tr_data->atlas_width * tr_data->atlas_height);
    if (!tr_data->texture_atlas_data) {
        log_error("Failed to allocate memory for texture atlas data");
        text_renderer_cleanup(renderer); // Используем свою же функцию очистки
        return 0;
    }

    glGenTextures(1, &tr_data->texture_atlas_id);
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tr_data->atlas_width, tr_data->atlas_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 4. Компиляция шейдеров
    tr_data->shader_program_textured = gl_shaders_create_program_from_sources(
        gl_shaders_textured_vertex_shader_source,
        gl_shaders_textured_fragment_shader_source
    );
    if (!tr_data->shader_program_textured) {
        log_error("Failed to create textured shader program for text");
        text_renderer_cleanup(renderer);
        return 0;
    }

    // 5. Создание VBO
    glGenBuffers(1, &tr_data->vbo);

    renderer->text_internal_data_private = tr_data;
    log_info("Text renderer initialized successfully");
    return 1;
}

void text_renderer_cleanup(Renderer* renderer) {
    if (!renderer || !renderer->text_internal_data_private) return;
    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;

    if (tr_data->shader_program_textured) glDeleteProgram(tr_data->shader_program_textured);
    if (tr_data->vbo) glDeleteBuffers(1, &tr_data->vbo);
    if (tr_data->texture_atlas_id) glDeleteTextures(1, &tr_data->texture_atlas_id);
    if (tr_data->texture_atlas_data) free(tr_data->texture_atlas_data);
    if (tr_data->ft_face) FT_Done_Face(tr_data->ft_face);
    if (tr_data->ft_library) FT_Done_FreeType(tr_data->ft_library);

    free(tr_data);
    renderer->text_internal_data_private = NULL;
    log_info("Text renderer cleaned up");
}

void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !renderer->text_internal_data_private || !text) {
        return;
    }
    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;

    if (!tr_data->is_freetype_initialized) {
        // Fallback: рисуем простой прямоугольник, если FreeType не работает
        float text_width = strlen(text) * 8.0f * scale; // Приблизительная ширина
        renderer_draw_quad(renderer, x, y, text_width, 16.0f * scale, 1, 0, 1, 1); // Ярко-розовый для отладки
        return;
    }

    float mvp[16];
    float w_render = (float)renderer_get_width(renderer);
    float h_render = (float)renderer_get_height(renderer);
    // Ортографическая проекция
    mvp[0] = 2.0f / w_render; mvp[5] = -2.0f / h_render; mvp[10] = 1; mvp[15] = 1;
    mvp[12] = -1; mvp[13] = 1;
    mvp[1] = mvp[2] = mvp[3] = mvp[4] = mvp[6] = mvp[7] = mvp[8] = mvp[9] = mvp[11] = mvp[14] = 0;

    float cursor_x = x;

    for (const char* p = text; *p; p++) {
        if (!load_glyph_into_atlas(tr_data, *p)) {
            continue;
        }

        struct glyph_cache_entry* glyph = &tr_data->glyph_cache[(unsigned char)*p - 32];
        
        float x_pos = cursor_x + glyph->bearing_x * scale;
        float y_pos = y - (glyph->height - glyph->bearing_y) * scale;
        float w = glyph->width * scale;
        float h = glyph->height * scale;

        gl_primitives_draw_textured_quad(
            tr_data->shader_program_textured,
            tr_data->texture_atlas_id,
            x_pos, y_pos, w, h,
            glyph->u0, glyph->v0, glyph->u1, glyph->v1,
            color,
            mvp,
            tr_data->vbo
        );

        cursor_x += glyph->advance_x * scale;
    }
}
