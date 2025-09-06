// src/gui/ui_manager_core.c
#include "see_code/gui/ui_manager.h"
#include "see_code/gui/renderer.h"
#include "see_code/gui/termux_gui_backend.h"
#include "see_code/gui/widgets.h" // Для новых виджетов
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // Для констант
#include <stdlib.h>
#include <string.h>

// Предполагаем, что эта функция существует в app.c для получения времени
extern unsigned long long app_get_time_millis(void);

// --- Внутренняя структура менеджера UI ---
// Определяем структуру здесь, так как она приватная для этого модуля
struct UIManager {
    Renderer* renderer; // Может быть NULL, если используется Termux-GUI
    TermuxGUIBackend* termux_backend; // Backend для fallback
    DiffData* diff_data;
    float scroll_y;
    float content_height;
    RendererType active_renderer;
    int needs_redraw;
    // --- ПОЛЯ ДЛЯ НОВЫХ ВИДЖЕТОВ ---
    TextInputState* input_field;  // Указатель на состояние текстового поля ввода
    ButtonState* menu_button;     // Указатель на состояние кнопки "..."
    // --- КОНЕЦ ПОЛЕЙ ДЛЯ НОВЫХ ВИДЖЕТОВ ---
};

// Вспомогательная функция для определения типа рендерера
static RendererType ui_manager_determine_renderer_type(const UIManager* ui_manager) {
    if (!ui_manager) return RENDERER_TYPE_UNKNOWN;
    if (ui_manager->renderer) return RENDERER_TYPE_GLES2;
    if (ui_manager->termux_backend) return RENDERER_TYPE_TERMUX_GUI;
    return RENDERER_TYPE_UNKNOWN;
}

UIManager* ui_manager_create(Renderer* renderer) {
    UIManager* ui_manager = malloc(sizeof(UIManager));
    if (!ui_manager) {
        log_error("Failed to allocate memory for UIManager");
        return NULL;
    }
    memset(ui_manager, 0, sizeof(UIManager));
    ui_manager->renderer = renderer;
    ui_manager->termux_backend = NULL; // Будет установлен позже, если используется fallback
    ui_manager->scroll_y = 0.0f;
    ui_manager->content_height = 0.0f;
    ui_manager->needs_redraw = 1;
    ui_manager->diff_data = NULL;
    ui_manager->active_renderer = ui_manager_determine_renderer_type(ui_manager);

    // --- ИНИЦИАЛИЗАЦИЯ ВИДЖЕТОВ ---
    // Получаем размеры окна для позиционирования виджетов
    float window_width = renderer ? renderer_get_width(renderer) : WINDOW_WIDTH_DEFAULT;
    float window_height = renderer ? renderer_get_height(renderer) : WINDOW_HEIGHT_DEFAULT;

    // Выделяем память для состояния виджетов
    ui_manager->input_field = malloc(sizeof(TextInputState));
    ui_manager->menu_button = malloc(sizeof(ButtonState));

    if (!ui_manager->input_field || !ui_manager->menu_button) {
        log_error("ui_manager_create: Failed to allocate memory for widgets");
        // Освободить уже выделенную память для виджетов
        if (ui_manager->input_field) free(ui_manager->input_field);
        if (ui_manager->menu_button) free(ui_manager->menu_button);
        ui_manager->input_field = NULL;
        ui_manager->menu_button = NULL;
        free(ui_manager);
        return NULL;
    }

    // Инициализируем текстовое поле внизу экрана
    if (!text_input_init(ui_manager->input_field, 
                         0.0f, // x
                         window_height - INPUT_FIELD_HEIGHT, // y
                         window_width, // width
                         INPUT_FIELD_HEIGHT // height
                        )) {
        log_error("ui_manager_create: Failed to initialize text input widget");
        // --- ИСПРАВЛЕНИЕ 3: Корректная очистка при ошибке ---
        // text_input_destroy вызывается ТОЛЬКО если text_input_init вернул успех.
        // В данном случае text_input_init вернул 0, значит, внутренние ресурсы TextInputState не были выделены.
        // Поэтому мы просто освобождаем саму структуру.
        free(ui_manager->input_field);
        free(ui_manager->menu_button);
        ui_manager->input_field = NULL;
        ui_manager->menu_button = NULL;
        free(ui_manager);
        return NULL;
        // --- КОНЕЦ ИСПРАВЛЕНИЯ ---
    }

    // Устанавливаем фокус на поле ввода при запуске
    text_input_set_focus(ui_manager->input_field, 1);

    // Инициализируем кнопку "..." в правом верхнем углу
    if (!button_init(ui_manager->menu_button,
                     window_width - MENU_BUTTON_SIZE - MARGIN, // x
                     MARGIN, // y
                     MENU_BUTTON_SIZE, // width
                     MENU_BUTTON_SIZE, // height
                     MENU_BUTTON_LABEL // label
                    )) {
        log_error("ui_manager_create: Failed to initialize menu button widget");
        // --- ИСПРАВЛЕНИЕ 3: Корректная очистка при ошибке ---
        // text_input_init для input_field прошел успешно, поэтому нужно вызвать text_input_destroy.
        text_input_destroy(ui_manager->input_field);
        // Освобождаем память под структуры виджетов
        free(ui_manager->input_field);
        free(ui_manager->menu_button);
        ui_manager->input_field = NULL;
        ui_manager->menu_button = NULL;
        free(ui_manager);
        return NULL;
        // --- КОНЕЦ ИСПРАВЛЕНИЯ ---
    }

    // --- КОНЕЦ ИНИЦИАЛИЗАЦИИ ВИДЖЕТОВ ---

    log_info("UIManager created successfully");
    return ui_manager;
}

void ui_manager_destroy(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }

    // --- ОЧИСТКА ВИДЖЕТОВ ---
    if (ui_manager->input_field) {
        text_input_destroy(ui_manager->input_field);
        free(ui_manager->input_field);
        ui_manager->input_field = NULL;
    }
    if (ui_manager->menu_button) {
        button_destroy(ui_manager->menu_button);
        free(ui_manager->menu_button);
        ui_manager->menu_button = NULL;
    }
    // --- КОНЕЦ ОЧИСТКИ ВИДЖЕТОВ ---

    // Освобождаем саму структуру
    free(ui_manager);
    log_info("UIManager destroyed");
}

void ui_manager_set_diff_data(UIManager* ui_manager, DiffData* data) {
    if (!ui_manager) {
        return;
    }
    ui_manager->diff_data = data;
    ui_manager->needs_redraw = 1; // Требуется перерисовка
    // content_height будет обновлен в ui_manager_update_layout
}

void ui_manager_update_layout(UIManager* ui_manager, float scroll_y) {
    if (!ui_manager) {
        return;
    }
    ui_manager->scroll_y = scroll_y;
    // Пересчитываем высоту содержимого
    // TODO: Реализовать логику расчета высоты содержимого
    // ui_manager->content_height = calculate_content_height(ui_manager);
    ui_manager->needs_redraw = 1; // Требуется перерисовка
}

// Функция для получения типа рендерера, с которым работает UI Manager
RendererType ui_manager_get_renderer_type(const UIManager* ui_manager) {
    return ui_manager_determine_renderer_type(ui_manager);
}

// Функция для установки типа рендерера (например, при переключении на fallback)
void ui_manager_set_renderer_type(UIManager* ui_manager, RendererType type) {
    if (!ui_manager) {
        return;
    }
    // Логика переключения типа рендерера может быть сложнее
    // и зависеть от того, как хранятся backend'ы
    log_warn("ui_manager_set_renderer_type: Not fully implemented");
    ui_manager->active_renderer = type;
    ui_manager->needs_redraw = 1; // Требуется перерисовка
}

// Функция для получения высоты содержимого (для прокрутки)
float ui_manager_get_content_height(UIManager* ui_manager) {
    if (!ui_manager) {
        return 0.0f;
    }
    // TODO: Реализовать логику расчета высоты содержимого
    // return ui_manager->content_height;
    return 1000.0f; // Заглушка
}
