// src/gui/ui_manager_input.c
#include "see_code/gui/ui_manager.h"
#include "see_code/gui/widgets.h" // Для новых виджетов
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>

// --- ОБНОВЛЕННЫЕ ФУНКЦИИ ОБРАБОТКИ ВВОДА ---

int ui_manager_handle_touch(UIManager* ui_manager, float x, float y) {
    if (!ui_manager) {
        return 0;
    }

    // --- ОБРАБОТКА СОБЫТИЙ ВИДЖЕТОВ ---
    // Попробуем сначала обработать событие виджетами
    int widget_handled = 0;
    if (ui_manager->input_field) {
        widget_handled = text_input_handle_click(ui_manager->input_field, x, y);
    }
    if (ui_manager->menu_button && !widget_handled) {
        widget_handled = button_handle_click(ui_manager->menu_button, x, y);
        if (widget_handled) {
            log_info("Menu button clicked!");
            // TODO: Добавить логику обработки нажатия кнопки (открытие меню и т.д.)
            // Например, можно вызвать callback или установить флаг в ui_manager
            // ui_manager->menu_button_pressed = 1;
        }
    }
    // Если событие было обработано виджетом, возвращаем 1
    if (widget_handled) {
        ui_manager->needs_redraw = 1; // Устанавливаем флаг перерисовки
        return 1;
    }
    // --- КОНЕЦ ОБРАБОТКИ СОБЫТИЙ ВИДЖЕТОВ ---

    // TODO: Добавить обработку кликов по diff-данным, если они есть
    // Это сложная логика, которая уже может существовать в оригинальном ui_manager_input.c
    // Пример:
    // if (ui_manager->diff_data) {
    //     // Логика обработки кликов по файлам/ханкам/строкам
    //     // ...
    //     // ui_manager->needs_redraw = 1; // Если состояние diff изменилось
    // }
    
    return widget_handled; // 0, если никто не обработал
}

// --- НОВАЯ ФУНКЦИЯ ДЛЯ ОБРАБОТКИ КЛАВИШ ---
void ui_manager_handle_key(UIManager* ui_manager, int key_code) {
    if (!ui_manager) {
        return;
    }
    // Передаем событие клавиши текстовому полю
    if (ui_manager->input_field) {
        int changed = text_input_handle_key(ui_manager->input_field, key_code);
        if (changed) {
             log_debug("Key event handled by text input widget, state changed");
             ui_manager->needs_redraw = 1; // Устанавливаем флаг перерисовки
        }
    }
    // TODO: Добавить обработку специальных клавиш
    // Например, для навигации по diff-данным, если фокус не на текстовом поле
    // if (!ui_manager->input_field || !ui_manager->input_field->is_focused) {
    //     switch(key_code) {
    //         case 0x10000: // Влево
    //             // Логика прокрутки или навигации
    //             break;
    //         case 0x10001: // Вправо
    //             // Логика прокрутки или навигации
    //             break;
    //         // ... другие клавиши
    //     }
    // }
}
// --- КОНЕЦ НОВОЙ ФУНКЦИИ ---
