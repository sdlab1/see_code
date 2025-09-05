// src/gui/ui_manager.c
#include "see_code/gui/ui_manager.h"
#include "see_code/gui/renderer.h" // Для renderer_draw_quad, renderer_draw_text, renderer_get_width/height
#include "see_code/gui/termux_gui_backend.h" // Для fallback рендерера
#include "see_code/data/diff_data.h" // Для DiffData, DiffFile, DiffHunk, DiffLine
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // Для цветов и размеров
#include <stdlib.h>
#include <string.h>
#include <math.h> // Для floorf

struct UIManager {
    Renderer* renderer;
    TermuxGUIBackend* termux_backend; // Backend для fallback рендерера
    DiffData* diff_data;
    float scroll_y;
    float content_height;
    RendererType active_renderer; // Отслеживаем, какой рендерер активен
    int needs_redraw; // Флаг, указывающий, что нужна перерисовка
};

UIManager* ui_manager_create(Renderer* renderer) {
    if (!renderer) {
        log_error("Invalid renderer provided to ui_manager_create");
        return NULL;
    }

    UIManager* ui_manager = malloc(sizeof(UIManager));
    if (!ui_manager) {
        log_error("Failed to allocate memory for UIManager");
        return NULL;
    }
    memset(ui_manager, 0, sizeof(UIManager));

    ui_manager->renderer = renderer;
    ui_manager->active_renderer = RENDERER_TYPE_GLES2; // По умолчанию GLES2

    // Пытаемся создать backend Termux-GUI для потенциального fallback'а
    // Проверяем, доступна ли библиотека
    if (termux_gui_backend_is_available()) {
        ui_manager->termux_backend = termux_gui_backend_create();
        if (ui_manager->termux_backend) {
             if (!termux_gui_backend_init(ui_manager->termux_backend)) {
                 log_warn("Failed to initialize Termux GUI backend, fallback will not be available");
                 termux_gui_backend_destroy(ui_manager->termux_backend);
                 ui_manager->termux_backend = NULL;
             } else {
                 log_info("Termux GUI backend created and initialized for potential fallback");
             }
        }
    } else {
        log_info("Termux GUI backend not available on this system");
    }

    return ui_manager;
}

void ui_manager_destroy(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }
    if (ui_manager->termux_backend) {
        termux_gui_backend_destroy(ui_manager->termux_backend);
    }
    free(ui_manager);
}

void ui_manager_set_diff_data(UIManager* ui_manager, DiffData* data) {
    if (!ui_manager) {
        return;
    }
    ui_manager->diff_data = data;
    ui_manager->needs_redraw = 1; // Запрашиваем перерисовку при изменении данных
}

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ РАСЧЕТА ВЫСОТЫ ---
void ui_manager_update_layout(UIManager* ui_manager, float scroll_y) {
    if (!ui_manager) {
        return;
    }
    ui_manager->scroll_y = scroll_y;
    // Рассчитываем высоту контента на основе diff данных, учитывая свернутые секции
    float total_height = 0.0f;
    const float line_height = LINE_HEIGHT;
    const float file_header_height = line_height;
    const float hunk_header_height = line_height;
    const float file_margin = 10.0f; // Отступы между файлами
    const float hunk_margin = 5.0f;  // Отступы между ханками

    if (ui_manager->diff_data) {
        for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
            DiffFile* file = &ui_manager->diff_data->files[i];
            total_height += file_header_height + file_margin;

            if (!file->is_collapsed) {
                for (size_t j = 0; j < file->hunk_count; j++) {
                    DiffHunk* hunk = &file->hunks[j];
                    total_height += hunk_header_height + hunk_margin;

                    if (!hunk->is_collapsed) {
                        total_height += hunk->line_count * line_height;
                    }
                }
            }
        }
    }
    ui_manager->content_height = total_height;
    log_debug("Updated layout: scroll_y=%.2f, content_height=%.2f", ui_manager->scroll_y, ui_manager->content_height);
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ РАСЧЕТА ВЫСОТЫ ---

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ РЕНДЕРИНГА ---
void ui_manager_render(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }

    if (ui_manager->active_renderer == RENDERER_TYPE_GLES2 && ui_manager->renderer) {
        // --- GLES2 Rendering Path ---
        log_debug("Rendering with GLES2");

        // Очищаем экран
        renderer_clear(
            ((COLOR_BACKGROUND >> 16) & 0xFF) / 255.0f,
            ((COLOR_BACKGROUND >> 8) & 0xFF) / 255.0f,
            (COLOR_BACKGROUND & 0xFF) / 255.0f,
            ((COLOR_BACKGROUND >> 24) & 0xFF) / 255.0f
        );

        // Рендерим diff данные, если они доступны
        if (ui_manager->diff_data) {
            // --- Обновленный рендеринг с учетом сворачивания и текста ---
            float y_pos = -ui_manager->scroll_y; // Учитываем прокрутку
            const float line_height = LINE_HEIGHT;
            const float hunk_header_height = line_height;
            const float file_header_height = line_height;
            const float margin = MARGIN;
            const float hunk_padding = HUNK_PADDING;
            const float screen_height = (float)renderer_get_height(ui_manager->renderer); // Получаем высоту экрана
            const float screen_width = (float)renderer_get_width(ui_manager->renderer);   // Получаем ширину экрана

            for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
                DiffFile* file = &ui_manager->diff_data->files[i];
                if (!file->path) continue;

                float file_y_start = y_pos;
                // Простая оптимизация рендеринга: не рисуем то, что точно вне экрана
                // 1. Если файл полностью ниже экрана, можно остановить рендеринг
                if (file_y_start > ui_manager->scroll_y + screen_height) {
                     log_debug("File %zu is below screen, stopping render", i);
                     break; // Все остальные файлы тоже будут ниже
                }

                // 2. Если файл полностью выше экрана, пропускаем его и все его содержимое
                // Рассчитаем приблизительную высоту файла, включая ханки и строки
                float file_height_estimate = file_header_height + margin;
                if (!file->is_collapsed) {
                    for (size_t fj = 0; fj < file->hunk_count; fj++) {
                        DiffHunk* fhunk = &file->hunks[fj];
                        file_height_estimate += hunk_header_height + hunk_padding;
                        if (!fhunk->is_collapsed) {
                            file_height_estimate += fhunk->line_count * line_height;
                        }
                        file_height_estimate += 5.0f; // hunk_margin
                    }
                }
                file_height_estimate += 10.0f; // file_margin

                if (y_pos + file_height_estimate < ui_manager->scroll_y) {
                     log_debug("File %zu is above screen, skipping", i);
                     y_pos += file_height_estimate; // Пропускаем, но учитываем его высоту
                     continue;
                }

                // Рисуем заголовок файла (с текстом!)
                float r_file = ((COLOR_FILE_HEADER >> 16) & 0xFF) / 255.0f;
                float g_file = ((COLOR_FILE_HEADER >> 8) & 0xFF) / 255.0f;
                float b_file = (COLOR_FILE_HEADER & 0xFF) / 255.0f;
                float a_file = ((COLOR_FILE_HEADER >> 24) & 0xFF) / 255.0f;
                renderer_draw_quad(ui_manager->renderer, margin, y_pos, screen_width - 2 * margin, file_header_height,
                                   r_file, g_file, b_file, a_file);
                // --- РИСУЕМ ТЕКСТ ЗАГОЛОВКА ФАЙЛА ---
                renderer_draw_text(ui_manager->renderer, file->path,
                                   margin + 5.0f, // Небольшой отступ слева
                                   y_pos + (file_header_height - 16.0f) / 2.0f, // Центрируем по вертикали
                                   1.0f, // Масштаб
                                   0xFFFFFFFF); // Белый цвет (0xAARRGGBB)
                // --- КОНЕЦ РИСОВАНИЯ ТЕКСТА ---
                y_pos += file_header_height;

                if (!file->is_collapsed) { // Только если файл развернут
                    for (size_t j = 0; j < file->hunk_count; j++) {
                        DiffHunk* hunk = &file->hunks[j];
                        if (!hunk->header) continue;

                        float hunk_y_start = y_pos;
                         // Простая оптимизация для ханков
                        // 1. Если ханк полностью выше экрана, пропускаем его и все его строки
                        // Рассчитаем приблизительную высоту ханка
                        float hunk_height_estimate = hunk_header_height + hunk_padding;
                        if (!hunk->is_collapsed) {
                            hunk_height_estimate += hunk->line_count * line_height;
                        }
                        hunk_height_estimate += 5.0f; // hunk_margin

                        if (y_pos + hunk_height_estimate < ui_manager->scroll_y) {
                             log_debug(" Hunk %zu in file %zu is above screen, skipping", j, i);
                             y_pos += hunk_height_estimate;
                             continue;
                        }

                        // Рисуем заголовок ханка (с текстом!)
                        float r_hunk = ((COLOR_HUNK_HEADER >> 16) & 0xFF) / 255.0f;
                        float g_hunk = ((COLOR_HUNK_HEADER >> 8) & 0xFF) / 255.0f;
                        float b_hunk = (COLOR_HUNK_HEADER & 0xFF) / 255.0f;
                        float a_hunk = ((COLOR_HUNK_HEADER >> 24) & 0xFF) / 255.0f;
                        renderer_draw_quad(ui_manager->renderer, margin + hunk_padding, y_pos, screen_width - 2 * (margin + hunk_padding), hunk_header_height,
                                           r_hunk, g_hunk, b_hunk, a_hunk);
                        // --- РИСУЕМ ТЕКСТ ЗАГОЛОВКА ХАНКА ---
                        renderer_draw_text(ui_manager->renderer, hunk->header,
                                           margin + hunk_padding + 5.0f,
                                           y_pos + (hunk_header_height - 16.0f) / 2.0f,
                                           1.0f,
                                           0xFF000000); // Черный цвет
                        // --- КОНЕЦ РИСОВАНИЯ ТЕКСТА ---
                        y_pos += hunk_header_height;

                        if (!hunk->is_collapsed) { // Только если ханк развернут
                            // Рисуем строки (с текстом!)
                            for (size_t k = 0; k < hunk->line_count; k++) {
                                DiffLine* line = &hunk->lines[k];
                                if (!line->content) continue;

                                // Еще одна оптимизация для строк
                                if (y_pos + line_height < ui_manager->scroll_y) {
                                     y_pos += line_height;
                                     continue;
                                }
                                if (y_pos > ui_manager->scroll_y + screen_height) {
                                     // Эта строка ниже экрана, остальные тоже будут
                                     log_debug(" Line %zu in hunk %zu is below screen, skipping remaining lines", k, j);
                                     y_pos += (hunk->line_count - k - 1) * line_height;
                                     break;
                                }

                                float r_line, g_line, b_line, a_line = 1.0f;
                                switch (line->type) {
                                    case LINE_TYPE_ADD:
                                        r_line = ((COLOR_ADD_LINE >> 16) & 0xFF) / 255.0f;
                                        g_line = ((COLOR_ADD_LINE >> 8) & 0xFF) / 255.0f;
                                        b_line = (COLOR_ADD_LINE & 0xFF) / 255.0f;
                                        break;
                                    case LINE_TYPE_DELETE:
                                        r_line = ((COLOR_DEL_LINE >> 16) & 0xFF) / 255.0f;
                                        g_line = ((COLOR_DEL_LINE >> 8) & 0xFF) / 255.0f;
                                        b_line = (COLOR_DEL_LINE & 0xFF) / 255.0f;
                                        break;
                                    default: // CONTEXT
                                        r_line = ((COLOR_CONTEXT_LINE >> 16) & 0xFF) / 255.0f;
                                        g_line = ((COLOR_CONTEXT_LINE >> 8) & 0xFF) / 255.0f;
                                        b_line = (COLOR_CONTEXT_LINE & 0xFF) / 255.0f;
                                        break;
                                }
                                renderer_draw_quad(ui_manager->renderer, margin + 2 * hunk_padding, y_pos, screen_width - 2 * (margin + 2 * hunk_padding), line_height,
                                                   r_line, g_line, b_line, a_line);
                                // --- РИСУЕМ ТЕКСТ СТРОКИ ---
                                // Ограничиваем длину для отображения
                                char display_buffer[105];
                                const char* display_text = line->content;
                                if (line->length > 100) {
                                    strncpy(display_buffer, line->content, 100);
                                    display_buffer[100] = '\0';
                                    strcat(display_buffer, "...");
                                    display_text = display_buffer;
                                }
                                renderer_draw_text(ui_manager->renderer, display_text,
                                                   margin + 2 * hunk_padding + 5.0f,
                                                   y_pos + (line_height - 16.0f) / 2.0f,
                                                   1.0f,
                                                   0xFF000000); // Черный цвет
                                // --- КОНЕЦ РИСОВАНИЯ ТЕКСТА ---
                                y_pos += line_height;
                            }
                        } // if (!hunk->is_collapsed)
                        // Добавляем немного отступа после ханка
                        y_pos += 5.0f;
                    }
                } // if (!file->is_collapsed)

                // Добавляем отступ после файла
                y_pos += 10.0f;
            }
        }

    } else if (ui_manager->active_renderer == RENDERER_TYPE_TERMUX_GUI && ui_manager->termux_backend) {
        // --- Termux GUI Fallback Rendering Path ---
        log_debug("Rendering with Termux GUI (Fallback)");

        if (ui_manager->diff_data) {
            termux_gui_backend_render_diff(ui_manager->termux_backend, ui_manager->diff_data);
            // В реальном приложении, вам также понадобится способ обработки событий от Termux GUI
            // Это может потребовать отдельного потока или интеграции в главный цикл
            // termux_gui_backend_handle_events(ui_manager->termux_backend);
        }
    } else {
        log_error("No valid renderer available for ui_manager_render");
    }
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ РЕНДЕРИНГА ---

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ ОБРАБОТКИ КЛИКОВ ---
// Предполагаем, что координаты клика уже преобразованы с учетом scroll_y
int ui_manager_handle_touch(UIManager* ui_manager, float x, float y) {
    if (!ui_manager || !ui_manager->diff_data) {
        return 0;
    }
    log_debug("UI Manager handling touch at (%.2f, %.2f)", x, y);

    // --- Логика обработки кликов для сворачивания ---
    // Это упрощенная версия. В реальном приложении нужно точно определять,
    // на какой элемент (файл или ханк) был клик.
    // Для демонстрации: клик в верхнюю часть экрана переключает первый файл,
    // клик в нижнюю часть переключает первый ханк первого файла.

    float local_y = y + ui_manager->scroll_y; // Преобразуем Y в координаты контента

    const float line_height = LINE_HEIGHT;
    const float file_header_height = line_height;
    const float hunk_header_height = line_height;
    const float margin = MARGIN;
    const float hunk_padding = HUNK_PADDING;

    float current_y = 0.0f;
    for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
        DiffFile* file = &ui_manager->diff_data->files[i];
        if (!file->path) continue;

        float file_y_start = current_y;
        current_y += file_header_height;

        // Проверяем, попал ли клик в заголовок файла
        if (local_y >= file_y_start && local_y < file_y_start + file_header_height) {
            file->is_collapsed = !file->is_collapsed;
            log_info("Toggled file '%s' collapse state to %s", file->path, file->is_collapsed ? "collapsed" : "expanded");
            ui_manager->needs_redraw = 1; // Запрашиваем перерисовку
            ui_manager_update_layout(ui_manager, ui_manager->scroll_y); // Обновляем высоту контента
            return 1; // Клик обработан
        }

        if (!file->is_collapsed) {
            for (size_t j = 0; j < file->hunk_count; j++) {
                DiffHunk* hunk = &file->hunks[j];
                if (!hunk->header) continue;

                float hunk_y_start = current_y;
                current_y += hunk_header_height;

                // Проверяем, попал ли клик в заголовок ханка
                if (local_y >= hunk_y_start && local_y < hunk_y_start + hunk_header_height) {
                    hunk->is_collapsed = !hunk->is_collapsed;
                    log_info("Toggled hunk '%.50s...' collapse state to %s", hunk->header, hunk->is_collapsed ? "collapsed" : "expanded");
                    ui_manager->needs_redraw = 1;
                    ui_manager_update_layout(ui_manager, ui_manager->scroll_y);
                    return 1;
                }

                if (!hunk->is_collapsed) {
                    current_y += hunk->line_count * line_height;
                }
                current_y += 5.0f; // hunk_margin
            }
        }
        current_y += 10.0f; // file_margin
    }

    // Если клик не по заголовку, можно обработать как обычную прокрутку или игнорировать
    log_debug("Touch did not hit a collapsible header");
    return 1; // Считаем, что обработан (пусть ui_manager в app.c решает, перерисовывать или нет)
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ ОБРАБОТКИ КЛИКОВ ---

float ui_manager_get_content_height(UIManager* ui_manager) {
    if (!ui_manager) {
        return 0.0f;
    }
    return ui_manager->content_height;
}

// --- ФУНКЦИИ ДЛЯ УПРАВЛЕНИЯ РЕНДЕРЕРОМ ---
void ui_manager_set_renderer_type(UIManager* ui_manager, RendererType type) {
    if (!ui_manager) return;

    if (type == RENDERER_TYPE_TERMUX_GUI && !ui_manager->termux_backend) {
        log_warn("Cannot switch to Termux GUI renderer: backend not available");
        return; // Cannot switch if backend is not available
    }

    // Выполняем любую необходимую очистку/настройку при переключении рендереров
    if (ui_manager->active_renderer == RENDERER_TYPE_GLES2 && type == RENDERER_TYPE_TERMUX_GUI) {
        log_info("Switching renderer from GLES2 to Termux GUI (Fallback)");
        // Очистка состояния GLES2 если нужно (например, текстуры)
    } else if (ui_manager->active_renderer == RENDERER_TYPE_TERMUX_GUI && type == RENDERER_TYPE_GLES2) {
        log_info("Switching renderer from Termux GUI (Fallback) to GLES2");
        // Очистка состояния Termux GUI если нужно
    }

    ui_manager->active_renderer = type;
    ui_manager->needs_redraw = 1; // Запрашиваем перерисовку при смене рендерера
}

RendererType ui_manager_get_renderer_type(const UIManager* ui_manager) {
    if (!ui_manager) return RENDERER_TYPE_GLES2; // Default/fail-safe
    return ui_manager->active_renderer;
}
// --- КОНЕЦ ФУНКЦИЙ ДЛЯ УПРАВЛЕНИЯ РЕНДЕРЕРОМ ---
