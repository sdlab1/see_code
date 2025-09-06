// src/gui/renderer/text_renderer.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h" // Для доступа к gl функциям и renderer_draw_quad
#include "see_code/gui/renderer/gl_context.h" // Для доступа к GLContext, если нужно
#include "see_code/gui/renderer/gl_shaders.h" // Для скомпилированных шейдеров
#include "see_code/gui/renderer/gl_primitives.h" // Для рисования квадратов
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // Для путей к шрифтам и констант
#include <stdlib.h>
#include <string.h>
// --- Добавлено для FreeType ---
#include <ft2build.h>
#include FT_FREETYPE_H
// --- Конец добавления ---

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

// --- Предполагаем, что эти функции определены в freetype_text_renderer_init.c и freetype_text_renderer_draw.c ---
// Так как они не существуют как extern, реализуем их здесь как статические inline-вызовы.
// В реальном проекте их лучше определить в соответствующих .c файлах и экспортировать.
static int freetype_text_renderer_init(Renderer* renderer, const char* font_path_hint);
static void freetype_text_renderer_cleanup(Renderer* renderer);
static void freetype_text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color);
// --- Конец предположений ---

int text_renderer_init(Renderer* renderer, const char* font_path_hint) {
    if (!renderer) return 0;

    // Делегируем инициализацию модулю freetype_text_renderer
    return freetype_text_renderer_init(renderer, font_path_hint);
}

void text_renderer_cleanup(Renderer* renderer) {
    if (!renderer) return;

    // Делегируем очистку модулю freetype_text_renderer
    freetype_text_renderer_cleanup(renderer);
}

void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text) {
        return;
    }

    // Делегируем рендеринг текста модулю freetype_text_renderer
    freetype_text_renderer_draw_text(renderer, text, x, y, scale, color);
}

// --- Реализация делегатов (временно здесь, позже перенесем в freetype_text_renderer_*.c) ---
// Эти функции должны быть определены в freetype_text_renderer_init.c и freetype_text_renderer_draw.c
// Но для совместимости с текущей структурой, реализуем их здесь.

static int freetype_text_renderer_init(Renderer* renderer, const char* font_path_hint) {
    // Копируем реализацию из freetype_text_renderer_init.c
    // (Предполагаем, что freetype_text_renderer_init.c существует и содержит эту логику)
    // Для демонстрации, реализуем базовую логику здесь.
    log_info("Initializing FreeType text renderer...");
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

    // Компилируем шейдерную программу для текста
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
    if (!init_freetype_internal(tr_data, font_to_try)) { // Предполагаем, что init_freetype_internal определена ниже
        log_warn("Failed to initialize FreeType with primary font: %s", font_to_try);
        // Пробуем fallback шрифт
        if (!init_freetype_internal(tr_data, TRUETYPE_FONT_PATH)) { // Предполагаем, что init_freetype_internal определена ниже
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
    renderer->text_internal_data_private = tr_data; // Не рекомендуется без модификации struct Renderer
    log_info("FreeType text renderer initialized");
    return 1;
}

static void freetype_text_renderer_cleanup(Renderer* renderer) {
    // Копируем реализацию из freetype_text_renderer_cleanup
    // (Предполагаем, что freetype_text_renderer_cleanup существует и содержит эту логику)
    // Для демонстрации, реализуем базовую логику здесь.
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
    log_debug("FreeType text renderer cleaned up");
}

static void freetype_text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    // Копируем реализацию из freetype_text_renderer_draw_text
    // (Предполагаем, что freetype_text_renderer_draw_text существует и содержит эту логику)
    // Для демонстрации, реализуем базовую логику здесь.
    if (!renderer || !text) {
        return;
    }

    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;
    if (!tr_data) {
        log_error("TextRendererInternalData is NULL in freetype_text_renderer_draw_text");
        return;
    }

    if (tr_data->is_freetype_initialized && tr_data->shader_program_textured) {
        // --- FULL FREEType RENDERING ---
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

            if (!load_glyph_into_atlas_internal(tr_data, c)) continue; // Предполагаем, что load_glyph_into_atlas_internal определена ниже

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

// --- Вспомогательные функции FreeType (дублируются из freetype_text_renderer_init.c для совместимости) ---
static int init_freetype_internal(struct TextRendererInternalData* tr_data, const char* font_path) {
    // Реализация init_freetype_internal из freetype_text_renderer_init.c
    // (Предполагаем, что freetype_text_renderer_init.c существует и содержит эту логику)
    // Для демонстрации, реализуем базовую логику здесь.
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
        FT_D
