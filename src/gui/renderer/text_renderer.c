// src/gui/renderer/text_renderer.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>

// --- УДАЛЕНО: struct TextRendererInternalData ---
// Структура перемещена в text_renderer_core.c и сделана приватной
// --- КОНЕЦ УДАЛЕНИЯ ---

int text_renderer_init(Renderer* renderer, const char* font_path_hint) {
    // Делегируем инициализацию в text_renderer_core.c
    extern int text_renderer_core_init(Renderer* renderer, const char* font_path_hint); // Предполагаемая функция
    return text_renderer_core_init(renderer, font_path_hint);
}

void text_renderer_cleanup(Renderer* renderer) {
    // Делегируем очистку в text_renderer_core.c
    extern void text_renderer_core_cleanup(Renderer* renderer); // Предполагаемая функция
    text_renderer_core_cleanup(renderer);
}

void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    // Делегируем рендеринг в text_renderer_draw.c
    extern void text_renderer_draw_impl(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color); // Предполагаемая функция
    text_renderer_draw_impl(renderer, text, x, y, scale, color);
}
