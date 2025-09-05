// src/gui/renderer/text_renderer.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h" // Для доступа к gl функциям и renderer_draw_quad
#include "see_code/gui/renderer/gl_context.h" // Для доступа к GLContext, если нужно
#include "see_code/gui/renderer/gl_shaders.h" // Для скомпилированных шейдеров
#include "see_code/gui/renderer/gl_primitives.h" // Для рисования квадратов
#include "see_code/gui/renderer/freetype_text_draw.h" // <<< ДОБАВЛЕНО для freetype_text_draw >>>
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // Для путей к шрифтам и констант
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h> // Для snprintf
#include <math.h>  // Для floorf

// ... (внутренняя структура TextRendererInternalData и вспомогательные функции остаются без изменений) ...

int text_renderer_init(Renderer* renderer, const char* font_path_hint) {
    // ... (реализация init остается без изменений) ...
    // Инициализирует FreeType, шейдеры, VBO и т.д.
    // Сохраняет указатель на внутренние данные в renderer->text_internal_data_private
    return 1; // или 0 при ошибке
}

void text_renderer_cleanup(Renderer* renderer) {
    // ... (реализация cleanup остается без изменений) ...
    // Освобождает ресурсы FreeType, шейдеров, VBO и т.д.
}

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ РЕНДЕРИНГА ТЕКСТА ---
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
        // --- ДЕЛЕГИРОВАНИЕ FREEType RENDERING ---
        log_debug("FreeType is initialized, delegating text rendering to freetype_text_draw");
        // Делегируем вызов модулю freetype_text_draw
        freetype_text_draw(renderer, text, x, y, scale, color);
        return;
        // --- КОНЕЦ ДЕЛЕГИРОВАНИЯ FREEType RENDERING ---
    } else {
        // --- FALLBACK RENDERING ---
        log_debug("FreeType not initialized, using placeholder for text: %.20s", text);
        float text_width = strlen(text) * 8.0f * scale;
        float text_height = 16.0f * scale;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        renderer_draw_quad(renderer, x, y, text_width, text_height, r, g, b, a);
        // --- КОНЕЦ FALLBACK RENDERING ---
    }
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ РЕНДЕРИНГА ТЕКСТА ---
