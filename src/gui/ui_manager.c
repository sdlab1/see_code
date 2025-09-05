// src/gui/ui_manager.c (фрагмент)
// ... (в начало файла, после других includes)
#include "see_code/data/diff_data.h" // Убедимся, что структуры видны
// ...

// ... (внутри ui_manager_render, заменить существующий блок рендеринга на этот)
void ui_manager_render(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }

    if (ui_manager->active_renderer == RENDERER_TYPE_GLES2 && ui_manager->renderer) {
        // --- GLES2 Rendering Path ---
        log_debug("Rendering with GLES2");

        // Clear screen
        renderer_clear(
            ((COLOR_BACKGROUND >> 16) & 0xFF) / 255.0f,
            ((COLOR_BACKGROUND >> 8) & 0xFF) / 255.0f,
            (COLOR_BACKGROUND & 0xFF) / 255.0f,
            ((COLOR_BACKGROUND >> 24) & 0xFF) / 255.0f
        );

        // Render diff data if available
        if (ui_manager->diff_data) {
            // --- Обновленный рендеринг с учетом сворачивания ---
            float y_pos = -ui_manager->scroll_y; // Учитываем прокрутку
            const float line_height = LINE_HEIGHT;
            const float hunk_header_height = line_height;
            const float file_header_height = line_height;
            const float margin = MARGIN;
            const float hunk_padding = HUNK_PADDING;

            for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
                DiffFile* file = &ui_manager->diff_data->files[i];
                if (!file->path) continue;

                float file_y_start = y_pos;
                // Рисуем заголовок файла
                // (Пока заглушка, позже будет renderer_draw_text)
                renderer_draw_quad(
                    margin, y_pos, 1080 - 2 * margin, file_header_height,
                    ((COLOR_FILE_HEADER >> 16) & 0xFF) / 255.0f,
                    ((COLOR_FILE_HEADER >> 8) & 0xFF) / 255.0f,
                    (COLOR_FILE_HEADER & 0xFF) / 255.0f,
                    ((COLOR_FILE_HEADER >> 24) & 0xFF) / 255.0f
                );
                y_pos += file_header_height;

                if (!file->is_collapsed) { // Только если файл развернут
                    for (size_t j = 0; j < file->hunk_count; j++) {
                        DiffHunk* hunk = &file->hunks[j];
                        if (!hunk->header) continue;

                        float hunk_y_start = y_pos;
                        // Рисуем заголовок ханка
                        // (Пока заглушка, позже будет renderer_draw_text)
                        renderer_draw_quad(
                            margin + hunk_padding, y_pos, 1080 - 2 * (margin + hunk_padding), hunk_header_height,
                            ((COLOR_HUNK_HEADER >> 16) & 0xFF) / 255.0f,
                            ((COLOR_HUNK_HEADER >> 8) & 0xFF) / 255.0f,
                            (COLOR_HUNK_HEADER & 0xFF) / 255.0f,
                            ((COLOR_HUNK_HEADER >> 24) & 0xFF) / 255.0f
                        );
                        y_pos += hunk_header_height;

                        if (!hunk->is_collapsed) { // Только если ханк развернут
                            // Рисуем строки
                            for (size_t k = 0; k < hunk->line_count; k++) {
                                DiffLine* line = &hunk->lines[k];
                                if (!line->content) continue;

                                float r, g, b, a = 1.0f;
                                switch (line->type) {
                                    case LINE_TYPE_ADD:
                                        r = ((COLOR_ADD_LINE >> 16) & 0xFF) / 255.0f;
                                        g = ((COLOR_ADD_LINE >> 8) & 0xFF) / 255.0f;
                                        b = (COLOR_ADD_LINE & 0xFF) / 255.0f;
                                        break;
                                    case LINE_TYPE_DELETE:
                                        r = ((COLOR_DEL_LINE >> 16) & 0xFF) / 255.0f;
                                        g = ((COLOR_DEL_LINE >> 8) & 0xFF) / 255.0f;
                                        b = (COLOR_DEL_LINE & 0xFF) / 255.0f;
                                        break;
                                    default: // CONTEXT
                                        r = ((COLOR_CONTEXT_LINE >> 16) & 0xFF) / 255.0f;
                                        g = ((COLOR_CONTEXT_LINE >> 8) & 0xFF) / 255.0f;
                                        b = (COLOR_CONTEXT_LINE & 0xFF) / 255.0f;
                                        break;
                                }
                                // (Пока заглушка, позже будет renderer_draw_text)
                                renderer_draw_quad(
                                    margin + 2 * hunk_padding, y_pos, 1080 - 2 * (margin + 2 * hunk_padding), line_height,
                                    r, g, b, a
                                );
                                y_pos += line_height;
                            }
                        } // if (!hunk->is_collapsed)
                        // Добавляем немного отступа после ханка
                        y_pos += 5.0f;
                    }
                } // if (!file->is_collapsed)

                // Добавляем отступ после файла
                y_pos += 10.0f;

                // --- Простая оптимизация рендеринга: не рисуем то, что вне экрана ---
                // Это базовая проверка, можно улучшить
                // if (file_y_start > ui_manager->config->window_height) break; // Все остальное ниже экрана
                // if (y_pos < 0) continue; // Этот файл еще не на экране
            }
        }

    } else if (ui_manager->active_renderer == RENDERER_TYPE_TERMUX_GUI && ui_manager->termux_backend) {
        // --- Termux GUI Fallback Rendering Path ---
        log_debug("Rendering with Termux GUI (Fallback)");

        if (ui_manager->diff_data) {
            termux_gui_backend_render_diff(ui_manager->termux_backend, ui_manager->diff_data);
        }
    } else {
        log_error("No valid renderer available for ui_manager_render");
    }
}
// --- Конец обновленного ui_manager_render ---

// ... (внутри ui_manager_update_layout, заменить существующий блок на этот)
void ui_manager_update_layout(UIManager* ui_manager, float scroll_y) {
    if (!ui_manager) {
        return;
    }
    ui_manager->scroll_y = scroll_y;
    // Calculate content height based on diff data, considering collapsed sections
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
// --- Конец обновленного ui_manager_update_layout ---

// ... (добавить новую функцию для обработки кликов на заголовках)
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
        if (y >= file_y_start - ui_manager->scroll_y && y < file_y_start + file_header_height - ui_manager->scroll_y) {
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
                if (y >= hunk_y_start - ui_manager->scroll_y && y < hunk_y_start + hunk_header_height - ui_manager->scroll_y) {
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
// --- Конец новой функции ui_manager_handle_touch ---
