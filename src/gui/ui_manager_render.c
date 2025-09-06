// src/gui/ui_manager_render.c
#include "see_code/gui/ui_manager.h"
#include "see_code/gui/renderer.h"
#include "see_code/gui/termux_gui_backend.h"
#include "see_code/gui/widgets.h" // Для новых виджетов
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // Для констант
#include "see_code/data/diff_data.h"
#include <stdlib.h>
#include <string.h>
#include <math.h> // Для fminf, fmaxf

// Предполагаем, что эта функция существует в app.c для получения времени
extern unsigned long long app_get_time_millis(void);

// Вспомогательная функция для определения типа рендерера
static RendererType ui_manager_determine_renderer_type(const UIManager* ui_manager) {
    if (!ui_manager) return RENDERER_TYPE_UNKNOWN;
    if (ui_manager->renderer) return RENDERER_TYPE_GLES2;
    if (ui_manager->termux_backend) return RENDERER_TYPE_TERMUX_GUI;
    return RENDERER_TYPE_UNKNOWN;
}

// --- ОСНОВНАЯ ФУНКЦИЯ РЕНДЕРИНГА ---
void ui_manager_render(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }

    RendererType renderer_type = ui_manager_determine_renderer_type(ui_manager);

    if (renderer_type == RENDERER_TYPE_GLES2 && ui_manager->renderer) {
        // --- РЕНДЕРИНГ ЧЕРЕЗ GLES2 ---
        // 1. Очищаем экран
        renderer_clear(ui_manager->renderer, 0.1f, 0.1f, 0.1f, 1.0f); // Темно-серый фон

        // 2. Рендерим основной diff (если есть)
        // Предполагается, что есть функция для этого в renderer или другом модуле
        // renderer_render_diff(ui_manager->renderer, ui_manager->diff_data, ui_manager->scroll_y);
        // Или вызов существующей логики рендеринга diff
        if (ui_manager->diff_data && ui_manager->diff_data->file_count > 0) {
             // Вызов бы существующей функции рендеринга diff, например:
             // diff_renderer_render(ui_manager->renderer, ui_manager->diff_data, ui_manager->scroll_y);
             log_debug("Rendering diff data with GLES2 (placeholder)");
             // Для демонстрации просто нарисуем прямоугольник
             renderer_draw_quad(ui_manager->renderer, 10, 10, 100, 50, 0xFFFF0000); // Красный прямоугольник
        } else {
             // Рисуем сообщение, что diff пуст
             renderer_draw_text(ui_manager->renderer, "No diff data available", 50, 100, 1.0f, 0xFF000000, 300);
        }

        // 3. Рендерим виджеты
        if (ui_manager->input_field) {
            text_input_render(ui_manager->input_field, ui_manager->renderer);
        }
        if (ui_manager->menu_button) {
            button_render(ui_manager->menu_button, ui_manager->renderer);
        }
        
    } else if (renderer_type == RENDERER_TYPE_TERMUX_GUI && ui_manager->termux_backend) {
        // --- РЕНДЕРИНГ ЧЕРЕЗ TERMUX-GUI ---
        // Предполагается, что есть функция в termux_gui_backend для рендеринга
        // или ui_manager напрямую взаимодействует с TermuxGUIBackend
        
        // 1. Рендерим основной diff (если есть)
        // termux_gui_backend_render_diff(ui_manager->termux_backend, ui_manager->diff_data);
        // Или вызов существующей логики
        if (ui_manager->diff_data && ui_manager->diff_data->file_count > 0) {
             log_debug("Rendering diff data with Termux-GUI (placeholder)");
             // termux_gui_backend_add_text_view(...); // Пример
        } else {
             // termux_gui_backend_add_text_view(..., "No diff data available");
        }
        
        // 2. Рендерим виджеты
        // Нужно добавить функции в termux_gui_backend или ui_manager для создания
        // эквивалентов виджетов через Termux-GUI API
        if (ui_manager->input_field) {
            termux_gui_backend_render_text_input(ui_manager->termux_backend, ui_manager->input_field);
        }
        if (ui_manager->menu_button) {
            termux_gui_backend_render_button(ui_manager->termux_backend, ui_manager->menu_button);
        }
        
    } else {
        log_error("UIManager: No valid renderer available for rendering");
    }
    
    // Сброс флага перерисовки после рендеринга
    ui_manager->needs_redraw = 0;
}
// --- КОНЕЦ ОСНОВНОЙ ФУНКЦИИ РЕНДЕРИНГА ---
