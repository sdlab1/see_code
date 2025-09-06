// src/gui/renderer/text_renderer.c
// УЛУЧШЕНИЕ: Добавлена логика обрезки (clipping) текста, который не помещается на экран.
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

struct TextRendererInternalData {
    int is_freetype_initialized;
    FT_Library ft_library;
    FT_Face ft_face;
    
    GLuint texture_atlas_id;
    unsigned char* texture_atlas_data;
    int atlas_width;
    int atlas_height;
    int atlas_pen_x;
    int atlas_pen_y;
    int atlas_row_height;

    struct {
        int is_loaded;
        float u0, v0, u1, v1;
        int width, height;
        int bearing_x, bearing_y;
        int advance_x;
    } glyph_cache[96];
};

static int load_glyph_into_atlas(struct TextRendererInternalData* tr_data, unsigned long char_code) {
    if (!tr_data || !tr_data->is_freetype_initialized || char_code < 32 || char_code > 126) return 0;
    int cache_index = char_code - 32;
    if (tr_data->glyph_cache[cache_index].is_loaded) return 1;

    if (FT_Load_Char(tr_data->ft_face, char_code, FT_LOAD_RENDER)) return 0;

    FT_GlyphSlot slot = tr_data->ft_face->glyph;
    int w = slot->bitmap.width;
    int h = slot->bitmap.rows;

    if (tr_data->atlas_pen_x + w + 1 > tr_data->atlas_width) {
        tr_data->atlas_pen_y += tr_data->atlas_row_height + 1;
        tr_data->atlas_pen_x = 0;
        tr_data->atlas_row_height = 0;
    }

    if (tr_data->atlas_pen_y + h + 1 > tr_data->atlas_height) return 0;
    
    for (int y = 0; y < h; y++) {
        memcpy(tr_data->texture_atlas_data + (tr_data->atlas_pen_y + y) * tr_data->atlas_width + tr_data->atlas_pen_x,
               slot->bitmap.buffer + y * slot->bitmap.pitch, w);
    }
    
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

    tr_data->atlas_pen_x += w + 1;
    if (h > tr_data->atlas_row_height) tr_data->atlas_row_height = h;
    
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tr_data->atlas_width, tr_data->atlas_height, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return 1;
}

int text_renderer_init(Renderer* renderer, const char* font_path_hint) {
    struct TextRendererInternalData* tr_data = calloc(1, sizeof(struct TextRendererInternalData));
    if (!tr_data) return 0;
    
    if (FT_Init_FreeType(&tr_data->ft_library) != 0) {
        free(tr_data);
        return 0;
    }

    const char* fonts_to_try[] = { font_path_hint, FREETYPE_FONT_PATH, TRUETYPE_FONT_PATH, FALLBACK_FONT_PATH, NULL };
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
    tr_data->atlas_width = ATLAS_WIDTH_DEFAULT;
    tr_data->atlas_height = ATLAS_HEIGHT_DEFAULT;
    tr_data->texture_atlas_data = calloc(1, tr_data->atlas_width * tr_data->atlas_height);

    glGenTextures(1, &tr_data->texture_atlas_id);
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tr_data->atlas_width, tr_data->atlas_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    renderer->text_internal_data_private = tr_data;
    return 1;
}

void text_renderer_cleanup(Renderer* renderer) {
    if (!renderer || !renderer->text_internal_data_private) return;
    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;
    
    glDeleteTextures(1, &tr_data->texture_atlas_id);
    free(tr_data->texture_atlas_data);
    FT_Done_Face(tr_data->ft_face);
    FT_Done_FreeType(tr_data->ft_library);
    free(tr_data);
    renderer->text_internal_data_private = NULL;
}

// --- ИЗМЕНЕННАЯ ФУНКЦИЯ ---
void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, uint32_t color, float max_width) {
    if (!renderer || !renderer->text_internal_data_private || !text) return;
    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;

    if (!tr_data->is_freetype_initialized) return;

    float cursor_x = x;
    for (const char* p = text; *p; p++) {
        if (!load_glyph_into_atlas(tr_data, *p)) continue;
        
        struct glyph_cache_entry* glyph = &tr_data->glyph_cache[(unsigned char)*p - 32];
        
        // --- УЛУЧШЕНИЕ: Проверяем, помещается ли следующий символ на экране ---
        if (max_width > 0 && (cursor_x + glyph->advance_x * scale) > x + max_width) {
            // Можно добавить отрисовку "..." здесь, если нужно
            break; 
        }

        if (glyph->width > 0 && glyph->height > 0) {
            float x_pos = cursor_x + glyph->bearing_x * scale;
            float y_pos = y - (glyph->height - glyph->bearing_y) * scale;
            float w = glyph->width * scale;
            float h = glyph->height * scale;
            renderer_draw_textured_quad(renderer, x_pos, y_pos, w, h,
                                      glyph->u0, glyph->v0, glyph->u1, glyph->v1, color);
        }
        
        cursor_x += glyph->advance_x * scale;
    }
}

GLuint renderer_get_font_atlas_texture(const Renderer* renderer) {
    if (!renderer || !renderer->text_internal_data_private) return 0;
    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;
    return tr_data->texture_atlas_id;
}
