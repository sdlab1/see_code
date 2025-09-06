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
        if (ui_manager->diff_data && ui_manager->diff_data->file_count > 0) {
            float current_y = MARGIN - ui_manager->scroll_y; // Начальная позиция Y с учетом прокрутки
            const float screen_width = renderer_get_width(ui_manager->renderer);
            const float max_text_width = screen_width - 2 * MARGIN;

            for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
                const DiffFile* file = &ui_manager->diff_data->files[i];

                // Проверяем, виден ли файл на экране
                if (current_y > renderer_get_height(ui_manager->renderer) + HUNK_HEADER_HEIGHT) {
                    break; // Файл полностью ниже экрана, выходим из цикла
                }
                if (current_y < -FILE_HEADER_HEIGHT) {
                    // Файл полностью выше экрана, пропускаем его отрисовку, но увеличиваем current_y
                    current_y += FILE_HEADER_HEIGHT + MARGIN;
                    if (!file->is_collapsed) {
                        for (size_t j = 0; j < file->hunk_count; j++) {
                            const DiffHunk* hunk = &file->hunks[j];
                            current_y += HUNK_HEADER_HEIGHT + HUNK_PADDING;
                            if (!hunk->is_collapsed) {
                                current_y += hunk->line_count * LINE_HEIGHT + HUNK_PADDING;
                            }
                        }
                    }
                    current_y += MARGIN;
                    continue; // Переходим к следующему файлу
                }

                // Рисуем заголовок файла
                if (file->path) {
                    renderer_draw_quad(ui_manager->renderer,
                                       MARGIN, current_y,
                                       screen_width - 2 * MARGIN, FILE_HEADER_HEIGHT,
                                       COLOR_FILE_HEADER);
                    renderer_draw_text(ui_manager->renderer, file->path,
                                       MARGIN + 5, current_y + FILE_HEADER_HEIGHT - 5,
                                       1.0f, 0xFFFFFFFF, max_text_width);
                }
                current_y += FILE_HEADER_HEIGHT + MARGIN;

                if (!file->is_collapsed) {
                    for (size_t j = 0; j < file->hunk_count; j++) {
                        const DiffHunk* hunk = &file->hunks[j];

                        // Проверяем, виден ли ханк на экране
                        if (current_y > renderer_get_height(ui_manager->renderer) + LINE_HEIGHT) {
                            break; // Ханк полностью ниже экрана
                        }
                        if (current_y < -HUNK_HEADER_HEIGHT) {
                            // Ханк полностью выше экрана, пропускаем его отрисовку, но увеличиваем current_y
                            current_y += HUNK_HEADER_HEIGHT + HUNK_PADDING;
                            if (!hunk->is_collapsed) {
                                current_y += hunk->line_count * LINE_HEIGHT + HUNK_PADDING;
                            }
                            continue; // Переходим к следующему ханку
                        }

                        // Рисуем заголовок ханка
                        if (hunk->header) {
                            renderer_draw_quad(ui_manager->renderer,
                                               MARGIN + 10, current_y,
                                               screen_width - 2 * (MARGIN + 10), HUNK_HEADER_HEIGHT,
                                               COLOR_HUNK_HEADER);
                            renderer_draw_text(ui_manager->renderer, hunk->header,
                                               MARGIN + 15, current_y + HUNK_HEADER_HEIGHT - 5,
                                               1.0f, 0xFFFFFFFF, max_text_width - 10);
                        }
                        current_y += HUNK_HEADER_HEIGHT + HUNK_PADDING;

                        if (!hunk->is_collapsed) {
                            // Рисуем строки ханка
                            for (size_t k = 0; k < hunk->line_count; k++) {
                                const DiffLine* line = &hunk->lines[k];

                                // Проверяем, видна ли строка на экране
                                if (current_y > renderer_get_height(ui_manager->renderer)) {
                                    break; // Строка полностью ниже экрана
                                }
                                if (current_y < -LINE_HEIGHT) {
                                    // Строка полностью выше экрана, пропускаем ее отрисовку
                                    current_y += LINE_HEIGHT;
                                    continue;
                                }

                                uint32_t line_color = 0xFFFFFFFF; // Белый по умолчанию
                                uint32_t bg_color = COLOR_BACKGROUND; // Темный фон по умолчанию

                                switch (line->type) {
                                    case LINE_TYPE_ADD:
                                        line_color = 0xFF00FF00; // Зеленый
                                        bg_color = 0xFF002200;   // Темно-зеленый фон
                                        break;
                                    case LINE_TYPE_DELETE:
                                        line_color = 0xFFFF0000; // Красный
                                        bg_color = 0xFF220000;   // Темно-красный фон
                                        break;
                                    case LINE_TYPE_CONTEXT:
                                        line_color = 0xFFAAAAAA; // Серый
                                        bg_color = 0xFF111111;   // Почти черный фон
                                        break;
                                }

                                // Рисуем фон строки
                                renderer_draw_quad(ui_manager->renderer,
                                                   MARGIN + 20, current_y,
                                                   screen_width - 2 * (MARGIN + 20), LINE_HEIGHT,
                                                   bg_color);
                                // Рисуем текст строки
                                if (line->content && strlen(line->content) > 0) {
                                    // Обрезаем первый символ ('+', '-', ' ') для чистоты отображения
                                    const char* display_text = line->content[0] == ' ' || line->content[0] == '+' || line->content[0] == '-' ? line->content + 1 : line->content;
                                    renderer_draw_text(ui_manager->renderer, display_text,
                                                       MARGIN + 25, current_y + LINE_HEIGHT - 5,
                                                       1.0f, line_color, max_text_width - 20);
                                }
                                current_y += LINE_HEIGHT;
                            }
                            current_y += HUNK_PADDING; // Дополнительный отступ после строк ханка
                        } // if (!hunk->is_collapsed)
                    } // for (hunk)
                } // if (!file->is_collapsed)
                current_y += MARGIN; // Отступ после файла
            } // for (file)
        } else {
            // Рисуем сообщение, что diff пуст
            renderer_draw_text(ui_manager->renderer, "No diff data available", 50, 100, 1.0f, 0xFFFFFFFF, 300);
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
        // 1. Рендерим основной diff (если есть)
        if (ui_manager->diff_data && ui_manager->diff_data->file_count > 0) {
            termux_gui_backend_render_diff(ui_manager->termux_backend, ui_manager->diff_data);
        } else {
            // Можно добавить отрисовку сообщения "No diff data" через Termux-GUI
            // Это требует дополнительной функции в termux_gui_backend
        }

        // 2. Рендерим виджеты
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
