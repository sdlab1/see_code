// src/gui/renderer/freetype_text_renderer_init.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h" // Для доступа к gl_ctx, если нужно
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // Для FREETYPE_FONT_PATH, TRUETYPE_FONT_PATH
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H

// Предполагаем, что эти функции определены в других файлах модуля
extern int text_renderer_init_atlas(struct TextRendererInternalData* tr_data);
extern void text_renderer_cleanup_atlas(struct TextRendererInternalData* tr_data);
extern const char* gl_shaders_textured_vertex_shader_source;
extern const char* gl_shaders_textured_fragment_shader_source;
GLuint gl_shaders_create_program_from_sources(const char* vertex_source, const char* fragment_source); // Предполагаем, что определена

int text_renderer_init(Renderer* renderer, const char* font_path) {
    if (!renderer) {
        log_error("Renderer is NULL in text_renderer_init");
        return 0;
    }

    struct TextRendererInternalData* tr_data = malloc(sizeof(struct TextRendererInternalData));
    if (!tr_data) {
        log_error("Failed to allocate memory for TextRendererInternalData");
        return 0;
    }
    memset(tr_data, 0, sizeof(struct TextRendererInternalData));

    // 1. Инициализируем FreeType библиотеку
    if (FT_Init_FreeType(&tr_data->ft_library)) {
        log_error("Could not init FreeType library");
        free(tr_data);
        return 0;
    }
    log_debug("FreeType library initialized");

    // 2. Загружаем шрифт
    const char* font_to_try = font_path ? font_path : FREETYPE_FONT_PATH;
    if (FT_New_Face(tr_data->ft_library, font_to_try, 0, &tr_data->ft_face)) {
        log_warn("Failed to load primary font from %s", font_to_try);
        FT_Done_FreeType(tr_data->ft_library);
        free(tr_data);
        return 0;
    } else {
        log_info("FreeType initialized with font: %s", font_to_try);
    }

    // 3. Устанавливаем размер шрифта
    if (FT_Set_Pixel_Sizes(tr_data->ft_face, 0, 16)) {
        log_warn("Failed to set pixel sizes for font");
        // Не критично, продолжаем
    }

    // 4. Инициализируем атлас текстур
    if (!text_renderer_init_atlas(tr_data)) {
        log_error("Failed to initialize texture atlas");
        FT_Done_Face(tr_data->ft_face);
        FT_Done_FreeType(tr_data->ft_library);
        free(tr_data);
        return 0;
    }

    // 5. Компиляция шейдеров для текстурированного рендеринга
    tr_data->shader_program_textured = gl_shaders_create_program_from_sources(
        gl_shaders_textured_vertex_shader_source,
        gl_shaders_textured_fragment_shader_source
    );
    if (!tr_data->shader_program_textured) {
        log_error("Failed to create textured shader program for text renderer");
        text_renderer_cleanup_atlas(tr_data);
        FT_Done_Face(tr_data->ft_face);
        FT_Done_FreeType(tr_data->ft_library);
        free(tr_data);
        return 0;
    }

    // 6. Создание VBO для текстурированных квадратов
    glGenBuffers(1, &tr_data->vbo_textured);
    if (tr_data->vbo_textured == 0) {
        log_error("Failed to generate VBO for textured text");
        glDeleteProgram(tr_data->shader_program_textured);
        text_renderer_cleanup_atlas(tr_data);
        FT_Done_Face(tr_data->ft_face);
        FT_Done_FreeType(tr_data->ft_library);
        free(tr_data);
        return 0;
    }

    tr_data->is_freetype_initialized = 1;
    renderer->text_internal_data_private = tr_data;
    log_info("Text renderer initialized with FreeType");
    return 1;
}

void text_renderer_cleanup(Renderer* renderer) {
    if (!renderer) return;
    struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)renderer->text_internal_data_private;
    if (!tr_data) return;

    if (tr_data->shader_program_textured) {
        glDeleteProgram(tr_data->shader_program_textured);
        tr_data->shader_program_textured = 0;
    }
    if (tr_data->vbo_textured) {
        glDeleteBuffers(1, &tr_data->vbo_textured);
        tr_data->vbo_textured = 0;
    }
    text_renderer_cleanup_atlas(tr_data);

    if (tr_data->ft_face) {
        FT_Done_Face(tr_data->ft_face);
        tr_data->ft_face = NULL;
    }
    if (tr_data->ft_library) {
        FT_Done_FreeType(tr_data->ft_library);
        tr_data->ft_library = NULL;
    }
    tr_data->is_freetype_initialized = 0;
    free(tr_data);
    renderer->text_internal_data_private = NULL;
    log_info("FreeType resources cleaned up");
}
