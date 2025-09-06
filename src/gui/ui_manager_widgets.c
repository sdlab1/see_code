// src/gui/ui_manager_widgets.c
#include "see_code/gui/ui_manager.h"
#include "see_code/gui/widgets.h"
#include "see_code/gui/renderer.h"
#include "see_code/core/config.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>

// Предполагаем, что эта функция существует в app.c для получения времени
extern unsigned long long app_get_time_millis(void);

// --- Функции для работы с виджетами в UIManager ---

int ui_manager_widgets_init(UIManager* ui_manager) {
    if (!ui_manager) {
        log_error("ui_manager_widgets_init: ui_manager is NULL");
        return 0;
    }

    // Выделяем память для состояния виджетов
    ui_manager->input_field = malloc(sizeof(TextInputState));
    ui_manager->menu_button = malloc(sizeof(ButtonState));
    
    if (!ui_manager->input_field || !ui_manager->menu_button) {
        log_error("ui_manager_widgets_init: Failed to allocate memory for widgets");
        // Освободить уже выделенную память
        if (ui_manager->input_field) free(ui_manager->input_field);
        if (ui_manager->menu_button) free(ui_manager->menu_button);
        ui_manager->input_field = NULL;
        ui_manager->menu_button = NULL;
        return 0;
    }

    // Получаем размеры окна для позиционирования виджетов
    float window_width = DEFAULT_WINDOW_WIDTH;
    float window_height = DEFAULT_WINDOW_HEIGHT;
    if (ui_manager->renderer) {
        window_width = renderer_get_width(ui_manager->renderer);
        window_height = renderer_get_height(ui_manager->renderer);
    } else if (ui_manager->termux_backend) {
        // TODO: Получить размеры окна из TermuxGUIBackend, если возможно
        // Пока используем дефолтные
    }

    // Инициализируем текстовое поле внизу экрана
    if (!text_input_init(ui_manager->input_field, 
                         0.0f,                           // x
                         window_height - INPUT_FIELD_HEIGHT, // y
                         window_width,                   // width
                         INPUT_FIELD_HEIGHT              // height
                        )) {
        log_error("ui_manager_widgets_init: Failed to initialize text input widget");
        free(ui_manager->input_field);
        free(ui_manager->menu_button);
        ui_manager->input_field = NULL;
        ui_manager->menu_button = NULL;
        return 0;
    }
    // Устанавливаем фокус на поле ввода при запуске
    text_input_set_focus(ui_manager->input_field, 1);
    log_info("Text input widget initialized");

    // Инициализируем кнопку "..." в правом верхнем углу
    if (!button_init(ui_manager->menu_button,
                     window_width - MENU_BUTTON_SIZE - UI_MARGIN, // x
                     UI_MARGIN,                                   // y
                     MENU_BUTTON_SIZE,                            // width
                     MENU_BUTTON_SIZE,                            // height
                     MENU_BUTTON_LABEL                            // label
                    )) {
        log_error("ui_manager_widgets_init: Failed to initialize menu button widget");
        text_input_destroy(ui_manager->input_field);
        free(ui_manager->input_field);
        free(ui_manager->menu_button);
        ui_manager->input_field = NULL;
        ui_manager->menu_button = NULL;
        return 0;
    }
    log_info("Menu button widget initialized");

    return 1; // Успех
}

void ui_manager_widgets_destroy(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }

    if (ui_manager->input_field) {
        text_input_destroy(ui_manager->input_field);
        free(ui_manager->input_field);
        ui_manager->input_field = NULL;
        log_debug("Text input widget destroyed");
    }

    if (ui_manager->menu_button) {
        button_destroy(ui_manager->menu_button);
        free(ui_manager->menu_button);
        ui_manager->menu_button = NULL;
        log_debug("Menu button widget destroyed");
    }
}

void ui_manager_widgets_render(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }

    // Определяем тип активного рендерера
    RendererType active_renderer = ui_manager_get_renderer_type(ui_manager);

    if (active_renderer == RENDERER_TYPE_GLES2 && ui_manager->renderer) {
        // Рендерим виджеты через GLES2
        if (ui_manager->input_field) {
            text_input_render(ui_manager->input_field, ui_manager->renderer);
        }
        if (ui_manager->menu_button) {
            button_render(ui_manager->menu_button, ui_manager->renderer);
        }
    } else if (active_renderer == RENDERER_TYPE_TERMUX_GUI && ui_manager->termux_backend) {
        // TODO: Рендерим виджеты через Termux-GUI
        // Нужно будет вызвать соответствующие функции из termux_gui_backend
        // или добавить их туда
        log_debug("Rendering widgets with Termux-GUI (placeholder)");
        // termux_gui_backend_render_widgets(ui_manager->termux_backend, ui_manager->input_field, ui_manager->menu_button);
    } else {
        log_warn("ui_manager_widgets_render: No valid renderer available");
    }
}

int ui_manager_widgets_handle_touch(UIManager* ui_manager, float x, float y) {
    if (!ui_manager) {
        return 0;
    }

    int handled = 0;
    
    // Передаем событие клика текстовому полю
    if (ui_manager->input_field) {
        handled = text_input_handle_click(ui_manager->input_field, x, y);
        if (handled) {
            log_debug("Touch event handled by text input widget");
        }
    }
    
    // Передаем событие клика кнопке, если текстовое поле не обработало
    if (ui_manager->menu_button && !handled) {
        handled = button_handle_click(ui_manager->menu_button, x, y);
        if (handled) {
            log_info("Menu button clicked!");
            // TODO: Добавить логику обработки нажатия кнопки (открытие меню и т.д.)
            // Например, можно вызвать callback или установить флаг в ui_manager
        }
    }
    
    return handled;
}

void ui_manager_widgets_handle_key(UIManager* ui_manager, int key_code) {
    if (!ui_manager) {
        return;
    }
    
    // Передаем событие клавиши текстовому полю
    if (ui_manager->input_field) {
        int changed = text_input_handle_key(ui_manager->input_field, key_code);
        if (changed) {
            log_debug("Key event handled by text input widget, state changed");
            // Устанавливаем флаг перерисовки, если состояние виджета изменилось
            // Предполагаем, что в UIManager есть поле needs_redraw
            // ui_manager->needs_redraw = 1; 
        }
    }
    
    // TODO: Добавить обработку специальных клавиш, если они влияют на другие виджеты или UI в целом
}

// --- Геттеры для доступа к данным виджетов ---
const char* ui_manager_get_input_text(const UIManager* ui_manager) {
    if (!ui_manager || !ui_manager->input_field) {
        return NULL;
    }
    return text_input_get_text(ui_manager->input_field);
}

size_t ui_manager_get_input_text_length(const UIManager* ui_manager) {
    if (!ui_manager || !ui_manager->input_field) {
        return 0;
    }
    return text_input_get_length(ui_manager->input_field);
}

// --- Функции для управления фокусом (если понадобятся) ---
void ui_manager_set_widget_focus(UIManager* ui_manager, int widget_id) {
    if (!ui_manager) {
        return;
    }
    
    // Предположим, что widget_id: 0 - снять фокус, 1 - текстовое поле, 2 - кнопка
    switch(widget_id) {
        case 0: // Снять фокус со всех
            if (ui_manager->input_field) {
                text_input_set_focus(ui_manager->input_field, 0);
            }
            // У кнопки фокус, как правило, не нужен, но можно добавить
            break;
        case 1: // Фокус на текстовое поле
            if (ui_manager->input_field) {
                text_input_set_focus(ui_manager->input_field, 1);
            }
            break;
        case 2: // Фокус на кнопку (условно)
            // Пока не реализовано
            break;
        default:
            log_warn("ui_manager_set_widget_focus: Unknown widget_id %d", widget_id);
            break;
    }
}
