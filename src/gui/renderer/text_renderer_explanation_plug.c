// src/gui/renderer/text_renderer_explanation_plug.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>

// --- ЗАГЛУШКА text_renderer_draw_text ---
// Эта функция является placeholder'ом и должна быть заменена полноценной реализацией.
// Вместо рендеринга текста она рисует цветной прямоугольник.
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
        // TODO: Реализовать полноценный рендеринг через FreeType
        // Это сложная часть, требующая:
        // 1. Загрузки каждого глифа (FT_Load_Char)
        // 2. Обновления атласа текстур (если глиф новый)
        // 3. Расчета позиции и UV координат для каждого глифа
        // 4. Рисования серии текстурированных квадратов
        log_debug("FreeType is initialized, but full implementation is TODO. Using placeholder.");
        float text_width = strlen(text) * 8.0f * scale;
        float text_height = 16.0f * scale;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        renderer_draw_quad(renderer, x, y, text_width, text_height, r, g, b, a);
        return;
    }

    // Если FreeType не инициализирован, используем заглушку
    log_debug("FreeType not initialized, using placeholder for text: %.20s", text);
    float text_width = strlen(text) * 8.0f * scale;
    float text_height = 16.0f * scale;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    renderer_draw_quad(renderer, x, y, text_width, text_height, r, g, b, a);
}
// --- КОНЕЦ ЗАГЛУШКИ ---
