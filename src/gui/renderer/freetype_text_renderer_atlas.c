// src/gui/renderer/freetype_text_renderer_atlas.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define TEXTURE_ATLAS_WIDTH  1024
#define TEXTURE_ATLAS_HEIGHT 1024
#define GLYPH_PADDING 1

// Предполагаем, что эти структуры определены в text_renderer.c или приватном заголовке
// struct glyph_cache_entry {
//     int is_loaded;
//     float u0, v0, u1, v1;
//     int width, height;
//     int bearing_x, bearing_y;
//     int advance_x;
// };
// struct TextRendererInternalData {
//     ...
//     struct glyph_cache_entry glyph_cache[96];
//     ...
// };

static void update_texture_atlas(struct TextRendererInternalData* tr_data) {
    if (!tr_data || !tr_data->texture_atlas_id || !tr_data->texture_atlas_data) return;
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tr_data->atlas_width, tr_data->atlas_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static int find_free_space_in_atlas(struct TextRendererInternalData* tr_data, int glyph_width, int glyph_height, int *out_x, int *out_y) {
    if (!tr_data || !out_x || !out_y) return 0;
    static int current_pen_x = 0;
    static int current_pen_y = 0;
    static int max_row_height = 0;

    if (current_pen_x + glyph_width + GLYPH_PADDING <= TEXTURE_ATLAS_WIDTH) {
        *out_x = current_pen_x;
        *out_y = current_pen_y;
        current_pen_x += glyph_width + GLYPH_PADDING;
        if (glyph_height + GLYPH_PADDING > max_row_height) {
            max_row_height = glyph_height + GLYPH_PADDING;
        }
        return 1;
    }

    current_pen_x = 0;
    current_pen_y += max_row_height;
    max_row_height = 0;

    if (current_pen_y + glyph_height + GLYPH_PADDING <= TEXTURE_ATLAS_HEIGHT &&
        current_pen_x + glyph_width + GLYPH_PADDING <= TEXTURE_ATLAS_WIDTH) {
        *out_x = current_pen_x;
        *out_y = current_pen_y;
        current_pen_x += glyph_width + GLYPH_PADDING;
        max_row_height = glyph_height + GLYPH_PADDING;
        return 1;
    }

    log_error("Texture atlas is full, cannot load more glyphs");
    return 0;
}

int load_glyph_into_atlas_internal(struct TextRendererInternalData* tr_data, unsigned long char_code) {
    if (!tr_data || !tr_data->is_freetype_initialized || char_code < 32 || char_code > 126) {
        return 0;
    }

    int cache_index = char_code - 32;
    if (cache_index < 0 || cache_index >= 96) return 0;

    if (tr_data->glyph_cache[cache_index].is_loaded) {
        return 1;
    }

    if (FT_Load_Char(tr_data->ft_face, char_code, FT_LOAD_RENDER)) {
        log_error("Failed to load glyph for character code %lu (U+%04lX)", char_code, char_code);
        return 0;
    }

    FT_GlyphSlot slot = tr_data->ft_face->glyph;
    int bitmap_width = slot->bitmap.width;
    int bitmap_rows = slot->bitmap.rows;

    if (bitmap_width == 0 || bitmap_rows == 0) {
        tr_data->glyph_cache[cache_index].is_loaded = 1;
        tr_data->glyph_cache[cache_index].width = bitmap_width;
        tr_data->glyph_cache[cache_index].height = bitmap_rows;
        tr_data->glyph_cache[cache_index].bearing_x = slot->bitmap_left;
        tr_data->glyph_cache[cache_index].bearing_y = slot->bitmap_top;
        tr_data->glyph_cache[cache_index].advance_x = slot->advance.x >> 6;
        tr_data->glyph_cache[cache_index].u0 = 0.0f;
        tr_data->glyph_cache[cache_index].v0 = 0.0f;
        tr_data->glyph_cache[cache_index].u1 = 0.0f;
        tr_data->glyph_cache[cache_index].v1 = 0.0f;
        log_debug("Loaded whitespace glyph U+%04lX ('%c')", char_code, (char)char_code);
        return 1;
    }

    int atlas_x, atlas_y;
    if (!find_free_space_in_atlas(tr_data, bitmap_width, bitmap_rows, &atlas_x, &atlas_y)) {
        return 0;
    }

    for (int y = 0; y < bitmap_rows; y++) {
        unsigned char *src_row = slot->bitmap.buffer + y * slot->bitmap.pitch;
        unsigned char *dst_row = tr_data->texture_atlas_data + (atlas_y + y) * tr_data->atlas_width + atlas_x;
        memcpy(dst_row, src_row, bitmap_width);
    }

    tr_data->glyph_cache[cache_index].is_loaded = 1;
    tr_data->glyph_cache[cache_index].width = bitmap_width;
    tr_data->glyph_cache[cache_index].height = bitmap_rows;
    tr_data->glyph_cache[cache_index].bearing_x = slot->bitmap_left;
    tr_data->glyph_cache[cache_index].bearing_y = slot->bitmap_top;
    tr_data->glyph_cache[cache_index].advance_x = slot->advance.x >> 6;
    tr_data->glyph_cache[cache_index].u0 = (float)atlas_x / (float)tr_data->atlas_width;
    tr_data->glyph_cache[cache_index].v0 = (float)atlas_y / (float)tr_data->atlas_height;
    tr_data->glyph_cache[cache_index].u1 = (float)(atlas_x + bitmap_width) / (float)tr_data->atlas_width;
    tr_data->glyph_cache[cache_index].v1 = (float)(atlas_y + bitmap_rows) / (float)tr_data->atlas_height;

    update_texture_atlas(tr_data);
    log_debug("Loaded glyph U+%04lX ('%c') into atlas at (%d,%d)", char_code, (char)char_code, atlas_x, atlas_y);
    return 1;
}

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
    if (tr_data->texture_atlas_id == 0) {
        log_error("Failed to generate texture ID for atlas");
        free(tr_data->texture_atlas_data);
        tr_data->texture_atlas_data = NULL;
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tr_data->atlas_width, tr_data->atlas_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    memset(tr_data->glyph_cache, 0, sizeof(tr_data->glyph_cache));
    log_info("Texture atlas initialized (%dx%d)", tr_data->atlas_width, tr_data->atlas_height);
    return 1;
}

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
