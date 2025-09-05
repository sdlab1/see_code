// src/gui/renderer/text_renderer_core.c
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

// --- Внутренняя структура данных для text_renderer ---
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

    FT_Set_Pixel_Sizes(tr_data->ft_face, 0, FONT_SIZE_DEFAULT); // Используем значение из config.h

    tr_data->atlas_width = ATLAS_WIDTH_DEFAULT;
    tr_data->atlas_height = ATLAS_HEIGHT_DEFAULT;
    tr_data->texture_atlas_data = calloc(1, tr_data->atlas_width * tr_data->atlas_height);
    if (!tr_data->texture_atlas_data) {
        log_error("Failed to allocate memory for texture atlas");
        FT_Done_Face(tr_data->ft_face);
        FT_Done_FreeType(tr_data->ft_library);
        return 0;
    }

    // Инициализируем кэш глифов
    memset(tr_data->glyph_cache, 0, sizeof(tr_data->glyph_cache));

    // Создаем OpenGL текстуру для атласа
    glGenTextures(1, &tr_data->texture_atlas_id);
    glBindTexture(GL_TEXTURE_2D, tr_data->texture_atlas_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Изначально атлас пустой
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tr_data->atlas_width, tr_data->atlas_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tr_data->texture_atlas_data);
    glBindTexture(GL_TEXTURE_2D, 0);

    tr_data->is_freetype_initialized = 1;
    log_info("FreeType initialized successfully with font %s", font_path);
    return 1;
}

int text_renderer_init(Renderer* renderer, const char* font_path_hint) {
    if (!renderer) return 0;

    // Выделяем память для внутренних данных text_renderer
    struct TextRendererInternalData* tr_data = calloc(1, sizeof(struct TextRendererInternalData));
    if (!tr_data) {
        log_error("Failed to allocate memory for TextRendererInternalData");
        return 0;
    }

    // --- Инициализация шейдеров для текста ---
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
        "uniform vec4 text_color;\n" // Цвет текста передается как uniform
        "void main() {\n"
        "  vec4 tex_color = texture2D(tex, frag_texcoord);\n"
        "  gl_FragColor = text_color * vec4(1.0, 1.0, 1.0, tex_color.a);\n" // Используем альфа из текстуры
        "}\n";

    tr_data->shader_program_textured = gl_shaders_create_program_from_sources(
        textured_vertex_shader_source,
        textured_fragment_shader_source
    );
    if (!tr_data->shader_program_textured) {
        log_warn("Failed to create textured shader program for text renderer");
    }
    log_debug("Text shader program compiled and linked successfully");

    // Создаем VBO для текстовых квадратов
    glGenBuffers(1, &tr_data->vbo);

    // --- Попытка инициализации FreeType ---
    log_info("Attempting to initialize FreeType...");
    const char* font_to_try = font_path_hint ? font_path_hint : FREETYPE_FONT_PATH; // Используем hint или default
    if (!init_freetype_internal(tr_data, font_to_try)) {
        log_warn("Failed to initialize FreeType with primary font: %s", font_to_try);
        // Пробуем fallback шрифт
        if (!init_freetype_internal(tr_data, TRUETYPE_FONT_PATH)) {
             log_warn("Failed to initialize FreeType with fallback font: %s", TRUETYPE_FONT_PATH);
             log_warn("Text rendering will use placeholder rectangles.");
             tr_data->is_freetype_initialized = 0; // Явно помечаем как не инициализированный
        } else {
            log_info("FreeType initialized with fallback font: %s", TRUETYPE_FONT_PATH);
        }
    } else {
        log_info("FreeType initialized with primary font: %s", font_to_try);
    }
    // --- Конец попытки инициализации FreeType ---

    // Сохраняем указатель на внутренние данные в основном рендерере
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
