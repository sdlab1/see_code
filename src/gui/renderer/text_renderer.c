// src/gui/renderer/text_renderer.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h"
#include "see_code/gui/renderer/gl_context.h"
#include "see_code/gui/renderer/gl_shaders.h"
#include "see_code/gui/renderer/gl_primitives.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

// --- Внутренняя структура данных для text_renderer ---
struct TextRendererInternalData {
    int is_freetype_initialized;
    FT_Library ft_library;
    FT_Face ft_face;
    GLuint texture_atlas_id;
    unsigned char* texture_atlas_data;
    int atlas_width;
    int atlas_height;
    struct {
        unsigned long unicode_char;
        float u0, v0, u1, v1;
        int width, height;
        int bearing_x, bearing_y;
        int advance_x;
        int is_loaded;
    } glyph_cache[96];
    GLuint shader_program_textured;
    GLuint vbo;
};

// --- Вспомогательные функции FreeType ---
static int init_freetype_internal(struct TextRendererInternalData* tr_data, const char* font_path) {
    if (!tr_data || !font_path) return 0;

    if (FT_Init_FreeType(&tr_data->ft_library)) {
        log_error("Could not init FreeType library");
        return 0;
    }
    log_debug("FreeType library initialized");

    if (FT_New_Face(tr_data->ft_library, font_path, 0, &tr_data->ft_face)) {
        log_error("Failed to load font from %s", font_path);
        FT_Done_FreeType(tr_data->ft_library);
        return 0;
    }
    log_debug("Font loaded from %s", font_path);

    FT_Set_Pixel_Sizes(tr_data->ft_face, 0, FONT_SIZE_DEFAULT);

    tr_data->atlas_width = ATLAS_WIDTH_DEFAULT;
    tr_data->atlas_height = ATLAS_HEIGHT_DEFAULT;
    tr_data->texture_atlas_data = calloc(1, tr_data->atlas_width * tr_data->atlas_height);
    if (!tr_data->texture_atlas_data) {
        log_error("Failed to allocate memory for texture atlas");
        FT_Done_Face(tr_data->ft_face);
        FT_Done_FreeType(tr_data->ft_library);
        return 0;
    }

    memset(tr_data->glyph_cache, 0, sizeof(tr_data->glyph_cache));

    glGenTextures(1, &tr_data->texture_atlas_id);
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tr_data->atlas_width, tr_data->atlas_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);
    glBindTexture(GL_TEXTURE_2D, 0);

    tr_data->is_freetype_initialized = 1;
    log_info("FreeType initialized successfully with font %s", font_path);
    return 1;
}

static int load_glyph_into_atlas_internal(struct TextRendererInternalData* tr_data, unsigned long char_code) {
    if (!tr_data || !tr_data->is_freetype_initialized) {
        return 0;
    }

    int cache_index = char_code - 32;
    if (cache_index < 0 || cache_index >= 96) {
        log_debug("Character U+%04lX is outside ASCII range 32-126, skipping", char_code);
        return 0;
    }

    if (tr_data->glyph_cache[cache_index].is_loaded) {
        return 1;
    }

    if (FT_Load_Char(tr_data->ft_face, char_code, FT_LOAD_RENDER)) {
        log_warn("Failed to load glyph for character U+%04lX", char_code);
        return 0;
    }

    FT_GlyphSlot slot = tr_data->ft_face->glyph;
    int bitmap_width = slot->bitmap.width;
    int bitmap_rows = slot->bitmap.rows;

    int pen_x = 0, pen_y = 0;
    int row_height = 0;
    int found_space = 0;

    for (int y = 0; y < tr_data->atlas_height - bitmap_rows; ) {
        int max_h_in_row = 0;
        for (int x = 0; x < tr_data->atlas_width - bitmap_width; ) {
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
                break;
            } else {
                x += bitmap_width + 1;
            }
        }
        if (found_space) {
            row_height = max_h_in_row;
            break;
        }
        y += row_height + 1;
        row_height = 0;
    }

    if (!found_space) {
        log_warn("No space left in texture atlas for glyph U+%04lX", char_code);
        return 0;
    }

    for (int row = 0; row < bitmap_rows; row++) {
        for (int col = 0; col < bitmap_width; col++) {
            int atlas_index = (pen_y + row) * tr_data->atlas_width + (pen_x + col);
            int bitmap_index = row * slot->bitmap.width + col;
            tr_data->texture_atlas_data[atlas_index] = slot->bitmap.buffer[bitmap_index];
        }
    }

    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tr_data->atlas_width, tr_data->atlas_height, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);
    glBindTexture(GL_TEXTURE_2D, 0);

    tr_data->glyph_cache[cache_index].unicode_char = char_code;
    tr_data->glyph_cache[cache_index].u0 = (float)pen_x / tr_data->atlas_width;
    tr_data->glyph_cache[cache_index].v0 = (float)pen_y / tr_data->atlas_height;
    tr_data->glyph_cache[cache_index].u1 = (float)(pen_x + bitmap_width) / tr_data->atlas_width;
    tr_data->glyph_cache[cache_index].v1 = (float)(pen_y + bitmap_rows) / tr_data->atlas_height;
    tr_data->glyph_cache[cache_index].width = bitmap_width;
    tr_data->glyph_cache[cache_index].height = bitmap_rows;
    tr_data->glyph_cache[cache_index].bearing_x = slot->bitmap_left;
    tr_data->glyph_cache[cache_index].bearing_y = slot->bitmap_top;
    tr_data->glyph_cache[cache_index].advance_x = slot->advance.x >> 6;
    tr_data->glyph_cache[cache_index].is_loaded = 1;

    log_debug("Loaded glyph U+%04lX into atlas at (%d,%d)", char_code, pen_x, pen_y);
    return 1;
}
// --- Конец вспомогательных функций FreeType ---

int text_renderer_init(Renderer* renderer, const char* font_path_hint) {
    if (!renderer) return 0;

    struct TextRendererInternalData* tr_data = calloc(1, sizeof(struct TextRendererInternalData));
    if (!tr_data) {
        log_error("Failed to allocate memory for TextRendererInternalData");
        return 0;
    }

    static const char* textured_vertex_shader_source =
        "#version 100\n"
        "attribute vec2 position;\n"
        "attribute vec2 texcoord;\n"
        "varying vec2 frag_texcoord;\n"
        "uniform mat4 mvp;\n"
        "void main() {\n"
        "  gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
        "  frag_texcoord = texcoord;\n"
        "}\n";

    static const char* textured_fragment_shader_source =
        "#version 100\n"
        "precision mediump float;\n"
        "varying vec2 frag_texcoord;\n"
        "uniform sampler2D tex;\n"
        "uniform vec4 text_color;\n"
        "void main() {\n"
        "  vec4 tex_color = texture2D(tex, frag_texcoord);\n"
        "  gl_FragColor = text_color * vec4(1.0, 1.0, 1.0, tex_color.a);\n"
        "}\n";

    tr_data->shader_program_textured = gl_shaders_create_program_from_sources(
        textured_vertex_shader_source,
        textured_fragment_shader_source
    );
    if (!tr_data->shader_program_textured) {
        log_warn("Failed to create textured shader program for text renderer");
    }
    log_debug("Text shader program compiled and linked successfully");

    glGenBuffers(1, &tr_data->vbo);

    log_info("Attempting to initialize FreeType...");
    const char* font_to_try = font_path_hint ? font_path_hint : FREETYPE_FONT_PATH;
    if (!init_freetype_internal(tr_data, font_to_try)) {
        log_warn("Failed to initialize FreeType with primary font: %s", font_to_try);
        if (!init_freetype_internal(tr_data, TRUETYPE_FONT_PATH)) {
             log_warn("Failed to initialize FreeType with fallback font: %s", TRUETYPE_FONT_PATH);
             log_warn("Text rendering will use placeholder rectangles.");
             tr_data->is_freetype_initialized = 0;
        } else {
            log_info("FreeType initialized with fallback font: %s", TRUETYPE_FONT_PATH);
        }
    } else {
        log_info("FreeType initialized with primary font: %s", font_to_try);
    }

    renderer->text_internal_data_private = tr_data;
    log_info("Text renderer initialized");
    return 1;
}

void text_renderer_cleanup(Renderer* renderer) {
    if (!renderer) return;
    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;
    if (!tr_data) return;

    if (tr_data->shader_program_textured) glDeleteProgram(tr_data->shader_program_textured);
    if (tr_data->vbo) glDeleteBuffers(1, &tr_data->vbo);

    if (tr_data->texture_atlas_id) glDeleteTextures(1, &tr_data->texture_atlas_id);
    if (tr_data->texture_atlas_data) free(tr_data->texture_atlas_data);
    if (tr_data->ft_face) FT_Done_Face(tr_data->ft_face);
    if (tr_data->ft_library) FT_Done_FreeType(tr_data->ft_library);
    tr_data->is_freetype_initialized = 0;
    free(tr_data);
    renderer->text_internal_data_private = NULL;
    log_debug("Text renderer cleaned up");
}

// --- УДАЛЕНА РЕАЛИЗАЦИЯ text_renderer_draw_text ---
// Реализация перенесена в src/gui/renderer/text_renderer_explanation_plug.c
// --- КОНЕЦ УДАЛЕННОЙ РЕАЛИЗАЦИИ ---
