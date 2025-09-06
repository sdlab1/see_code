// src/gui/renderer.c
#include "see_code/gui/renderer.h"
#include "see_code/gui/renderer/gl_context.h"
#include "see_code/gui/renderer/gl_shaders.h"
#include "see_code/gui/renderer/gl_primitives.h"
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>

struct Renderer {
    GLContext* gl_ctx;
    int width;
    int height;
    // --- Данные для text_renderer ---
    void* text_internal_data_private; // Указатель на внутренние данные text_renderer
    // --- Конец данных для text_renderer ---
};

Renderer* renderer_create(int width, int height) {
    log_info("Creating modular renderer with dimensions %dx%d", width, height);

    Renderer* renderer = malloc(sizeof(Renderer));
    if (!renderer) {
        log_error("Failed to allocate memory for Renderer");
        return NULL;
    }
    memset(renderer, 0, sizeof(Renderer));
    renderer->width = width;
    renderer->height = height;

    // 1. Инициализируем модуль контекста OpenGL
    renderer->gl_ctx = gl_context_create(width, height);
    if (!renderer->gl_ctx) {
        log_error("Failed to create GL context module");
        free(renderer);
        return NULL;
    }
    log_debug("GL Context module created");

    // 2. Инициализируем модуль рендеринга текста
    if (!text_renderer_init(renderer, FREETYPE_FONT_PATH)) {
        log_warn("Failed to initialize text renderer with primary font, trying fallback");
        if (!text_renderer_init(renderer, TRUETYPE_FONT_PATH)) {
             log_error("Failed to initialize text renderer with fallback font");
        } else {
            log_info("Text renderer initialized with fallback font");
        }
    } else {
         log_info("Text renderer initialized with primary font");
    }

    log_info("Modular renderer created successfully");
    return renderer;
}

void renderer_destroy(Renderer* renderer) {
    if (!renderer) {
        return;
    }
    log_info("Destroying modular renderer");

    // 1. Очищаем модуль рендеринга текста
    text_renderer_cleanup(renderer);
    log_debug("Text renderer module cleaned up");

    // 2. Очищаем модуль контекста OpenGL
    if (renderer->gl_ctx) {
        gl_context_destroy(renderer->gl_ctx);
        log_debug("GL Context module destroyed");
    }

    free(renderer);
    log_info("Modular renderer destroyed");
}

int renderer_begin_frame(Renderer* renderer) {
    if (!renderer || !renderer->gl_ctx) {
        return 0;
    }
    return gl_context_begin_frame(renderer->gl_ctx);
}

int renderer_end_frame(Renderer* renderer) {
    if (!renderer || !renderer->gl_ctx) {
        return 0;
    }
    return gl_context_end_frame(renderer->gl_ctx);
}

void renderer_resize(Renderer* renderer, int width, int height) {
    if (!renderer) {
        return;
    }
    log_debug("Renderer resize called: %dx%d -> %dx%d", renderer->width, renderer->height, width, height);
    renderer->width = width;
    renderer->height = height;
    if (renderer->gl_ctx) {
        gl_context_resize(renderer->gl_ctx, width, height);
    }
    log_info("Renderer resized to %dx%d", width, height);
}

void renderer_clear(float r, float g, float b, float a) {
    if (!renderer) { // Исправлено: проверяем renderer, а не несуществующую переменную
         log_debug("Renderer is NULL in renderer_clear");
         return;
    }
    // Делегируем очистку экрана модулю контекста
    gl_context_clear(renderer->gl_ctx, r, g, b, a);
}

// --- ИСПРАВЛЕНО: Реализация renderer_draw_quad ---
void renderer_draw_quad(Renderer* renderer, float x, float y, float width, float height,
                       float r, float g, float b, float a) {
    if (!renderer) { // Исправлено: проверяем renderer
         log_debug("Renderer is NULL in renderer_draw_quad");
         return;
    }
    // Создаем простую ортографическую матрицу MVP
    // В реальном приложении её лучше получать из контекста или передавать
    float mvp[16] = {
         2.0f / renderer->width, 0, 0, 0,
         0, -2.0f / renderer->height, 0, 0,
         0, 0, 1, 0,
        -1, 1, 0, 1
    };

    // Делегируем рисование цветного квадрата модулю примитивов
    gl_primitives_draw_solid_quad(
        gl_shaders_get_solid_program(), // Получаем скомпилированную программу
        x, y, width, height,
        r, g, b, a,
        mvp,
        gl_primitives_get_solid_vbo() // Получаем общий VBO
    );
}
// --- КОНЕЦ ИСПРАВЛЕНИЯ renderer_draw_quad ---

// --- ИСПРАВЛЕНО: Реализация renderer_draw_text ---
void renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text) {
        return;
    }
    // Делегируем рендеринг текста специальному модулю
    text_renderer_draw_text(renderer, text, x, y, scale, color);
}
// --- КОНЕЦ ИСПРАВЛЕНИЯ renderer_draw_text ---

int renderer_get_width(const Renderer* renderer) {
    return renderer ? renderer->width : 0;
}

int renderer_get_height(const Renderer* renderer) {
    return renderer ? renderer->height : 0;
}
