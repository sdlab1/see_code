// src/gui/widgets.c
#include "see_code/gui/widgets.h"
#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // Для snprintf, если нужно
#include <math.h>  // Для fminf, fmaxf

// --- TextInputState ---

int text_input_init(TextInputState* input, float x, float y, float width, float height) {
    if (!input) {
        log_error("text_input_init: input is NULL");
        return 0;
    }

    // Инициализируем поля структуры
    memset(input, 0, sizeof(TextInputState));
    input->x = x;
    input->y = y;
    input->width = width;
    input->height = height;
    input->multiline = 1; // Поддерживаем многострочный ввод

    // Выделяем начальный буфер, например, на 256 байт
    input->buffer_size = 256;
    input->buffer = malloc(input->buffer_size);
    if (!input->buffer) {
        log_error("text_input_init: Failed to allocate initial buffer of size %zu", input->buffer_size);
        // Обнуляем поля, чтобы text_input_destroy не пытался освободить неправильный указатель
        memset(input, 0, sizeof(TextInputState));
        return 0;
    }
    input->buffer[0] = '\0'; // Инициализируем пустую строку

    log_debug("TextInput initialized at (%.2f, %.2f) size (%.2f x %.2f)", x, y, width, height);
    return 1; // Успех
}

void text_input_destroy(TextInputState* input) {
    if (!input) {
        return;
    }
    if (input->buffer) {
        free(input->buffer);
        input->buffer = NULL;
    }
    input->buffer_size = 0;
    input->text_length = 0;
    input->cursor_pos = 0;
    log_debug("TextInput destroyed");
}

// Вспомогательная функция для увеличения буфера при необходимости
static int text_input_ensure_capacity(TextInputState* input, size_t required_capacity) {
    if (input->buffer_size >= required_capacity) {
        return 1; // Уже достаточно места
    }

    // Увеличиваем размер, например, в 2 раза или до required_capacity, смотря что больше
    size_t new_size = input->buffer_size * 2;
    if (new_size < required_capacity) {
        new_size = required_capacity;
    }
    // Ограничиваем максимальный размер, например, 10MB
    if (new_size > 10 * 1024 * 1024) {
        log_warn("TextInput buffer size limit reached (10MB)");
        return 0; // Не удается увеличить
    }

    char* new_buffer = realloc(input->buffer, new_size);
    if (!new_buffer) {
        log_error("text_input_ensure_capacity: Failed to reallocate buffer to size %zu", new_size);
        return 0; // Не удалось увеличить
    }

    input->buffer = new_buffer;
    input->buffer_size = new_size;
    log_debug("TextInput buffer reallocated to size %zu", new_size);
    return 1;
}

// Вспомогательная функция для вставки символа в буфер
static int text_input_insert_char(TextInputState* input, size_t pos, char c) {
    // Проверяем, нужно ли увеличить буфер (+1 для нового символа, +1 для '\0')
    if (!text_input_ensure_capacity(input, input->text_length + 2)) {
        return 0; // Не удалось увеличить
    }

    // Сдвигаем часть строки вправо, чтобы освободить место
    memmove(input->buffer + pos + 1, input->buffer + pos, input->text_length - pos);
    // Вставляем новый символ
    input->buffer[pos] = c;
    // Увеличиваем длину текста
    input->text_length++;
    // Обновляем завершающий ноль
    input->buffer[input->text_length] = '\0';
    return 1;
}

// Вспомогательная функция для удаления символа из буфера
static int text_input_delete_char(TextInputState* input, size_t pos) {
     if (pos >= input->text_length) {
         return 0; // Неверная позиция
     }
     // Сдвигаем часть строки влево, перезаписывая удаляемый символ
     memmove(input->buffer + pos, input->buffer + pos + 1, input->text_length - pos - 1);
     // Уменьшаем длину текста
     input->text_length--;
     // Обновляем завершающий ноль
     input->buffer[input->text_length] = '\0';
     return 1;
}

int text_input_handle_key(TextInputState* input, int key_code) {
    if (!input || !input->is_focused) {
        return 0; // Игнорируем, если не инициализировано или не в фокусе
    }

    int changed = 0; // Флаг, изменилось ли состояние

    if (key_code >= 32 && key_code <= 126) { // Печатаемые ASCII символы
        // Вставляем символ в позицию курсора
        if (text_input_insert_char(input, input->cursor_pos, (char)key_code)) {
            input->cursor_pos++;
            changed = 1;
        }
    } else if (key_code == '\b') { // Backspace
        if (input->cursor_pos > 0) {
            input->cursor_pos--; // Перемещаем курсор влево
            if (text_input_delete_char(input, input->cursor_pos)) {
                changed = 1;
            }
        }
    } else if (key_code == 127) { // Delete (ASCII DEL)
        if (input->cursor_pos < input->text_length) {
            if (text_input_delete_char(input, input->cursor_pos)) {
                changed = 1;
            }
        }
    } else if (key_code == '\n' || key_code == '\r') { // Enter/Return
        if (input->multiline) {
            // Вставляем символ новой строки
            if (text_input_insert_char(input, input->cursor_pos, '\n')) {
                input->cursor_pos++;
                changed = 1;
            }
        } // Если не multiline, игнорируем Enter
    } else if (key_code == 27) { // Escape
        // Можно снять фокус, но для поля ввода это не обязательно
        // input->is_focused = 0;
        // changed = 1;
    } else if (key_code == 0x10000) { // Влево (предположим, это специальный код)
         if (input->cursor_pos > 0) {
             input->cursor_pos--;
             changed = 1; // Даже если только курсор переместился
         }
    } else if (key_code == 0x10010) { // Вправо (предположим, это специальный код)
         if (input->cursor_pos < input->text_length) {
             input->cursor_pos++;
             changed = 1;
         }
    }
    // TODO: Добавить обработку стрелок вверх/вниз для многострочного поля
    // TODO: Добавить обработку Home/End

    if (changed) {
        log_debug("TextInput state changed: length=%zu, cursor=%zu", input->text_length, input->cursor_pos);
    }
    return changed;
}

int text_input_handle_click(TextInputState* input, float mouse_x, float mouse_y) {
    if (!input) {
        return 0;
    }

    int was_focused = input->is_focused;
    // Проверяем, попал ли клик в область поля ввода
    if (mouse_x >= input->x && mouse_x <= input->x + input->width &&
        mouse_y >= input->y && mouse_y <= input->y + input->height) {
        input->is_focused = 1;
        // TODO: Установить cursor_pos в позицию клика (требует сложной логики определения позиции по координатам)
        log_debug("TextInput clicked and focused");
    } else {
        // Клик вне поля, можно снять фокус, но для этого приложения, возможно, не нужно
        // input->is_focused = 0;
    }

    return (was_focused != input->is_focused) ? 1 : 0; // Вернуть 1, если фокус изменился
}

const char* text_input_get_text(const TextInputState* input) {
    if (!input || !input->buffer) {
        return ""; // Возвращаем пустую строку вместо NULL
    }
    return input->buffer;
}

size_t text_input_get_length(const TextInputState* input) {
    if (!input) {
        return 0;
    }
    return input->text_length;
}

void text_input_set_focus(TextInputState* input, int focused) {
    if (!input) {
        return;
    }
    input->is_focused = focused ? 1 : 0;
    log_debug("TextInput focus set to %d", input->is_focused);
}

// Вспомогательная функция для получения времени в миллисекундах (заглушка)
// В реальном коде нужно использовать функцию из app.c или другую
// Предполагаем, что есть функция app_get_time_millis()
extern unsigned long long app_get_time_millis(void); // Предварительное объявление

void text_input_render(const TextInputState* input, Renderer* renderer) {
    if (!input || !renderer) {
        return;
    }

    // 1. Отрисовка фона поля ввода
    renderer_draw_quad(renderer, input->x, input->y, input->width, input->height, INPUT_FIELD_BACKGROUND_COLOR);

    // 2. Отрисовка рамки поля ввода
    renderer_draw_quad(renderer, input->x, input->y, input->width, input->height, INPUT_FIELD_BORDER_COLOR);

    // 3. Определение области для текста (с отступами)
    const float padding = 5.0f;
    float text_area_x = input->x + padding;
    float text_area_y = input->y + padding;
    float text_area_width = input->width - 2 * padding;
    float text_area_height = input->height - 2 * padding;

    // 4. Отрисовка текста или подсказки
    if (input->text_length > 0) {
        // Отрисовываем текст
        // TODO: Обработка многострочности и прокрутки
        renderer_draw_text(renderer, input->buffer, text_area_x, text_area_y + 15.0f, 1.0f, INPUT_FIELD_TEXT_COLOR, text_area_width);
    } else if (input->is_focused) {
        // Отрисовываем серую подсказку, если текста нет и поле в фокусе
        renderer_draw_text(renderer, INPUT_FIELD_PLACEHOLDER_TEXT, text_area_x, text_area_y + 15.0f, 1.0f, INPUT_FIELD_PLACEHOLDER_COLOR, text_area_width);
    }

    // 5. Отрисовка курсора, если поле в фокусе
    if (input->is_focused) {
        // Простая логика мерцания: курсор виден 500мс, невидим 500мс
        unsigned long long current_time_ms = app_get_time_millis();
        int cursor_visible = (current_time_ms / INPUT_FIELD_CURSOR_BLINK_INTERVAL_MS) % 2;

        if (cursor_visible) {
            // TODO: Рассчитать точную позицию X,Y курсора на основе input->cursor_pos и ширины символов
            // Пока просто рисуем вертикальную линию в начале
            float cursor_x = text_area_x; // + ширина текста до cursor_pos
            float cursor_y = text_area_y;
            renderer_draw_quad(renderer, cursor_x, cursor_y, INPUT_FIELD_CURSOR_WIDTH, 20.0f, INPUT_FIELD_CURSOR_COLOR);
        }
    }
}


// --- ButtonState ---

int button_init(ButtonState* button, float x, float y, float width, float height, const char* label) {
    if (!button) {
        log_error("button_init: button is NULL");
        return 0;
    }

    memset(button, 0, sizeof(ButtonState));
    button->x = x;
    button->y = y;
    button->width = width;
    button->height = height;

    if (label) {
        size_t label_len = strlen(label);
        button->label = malloc(label_len + 1);
        if (!button->label) {
            log_error("button_init: Failed to allocate memory for label");
            return 0;
        }
        strcpy(button->label, label);
    } else {
        button->label = NULL;
    }

    log_debug("Button initialized at (%.2f, %.2f) size (%.2f x %.2f) with label '%s'",
              x, y, width, height, label ? label : "NULL");
    return 1;
}

void button_destroy(ButtonState* button) {
    if (!button) {
        return;
    }
    if (button->label) {
        free(button->label);
        button->label = NULL;
    }
    log_debug("Button destroyed");
}

int button_handle_click(ButtonState* button, float mouse_x, float mouse_y) {
    if (!button) {
        return 0;
    }

    int was_pressed = button->is_pressed;
    // Проверяем, попал ли клик в область кнопки
    if (mouse_x >= button->x && mouse_x <= button->x + button->width &&
        mouse_y >= button->y && mouse_y <= button->y + button->height) {
        button->is_pressed = 1;
        log_debug("Button '%s' pressed", button->label ? button->label : "No Label");
        return 1; // Кнопка нажата
    } else {
        button->is_pressed = 0; // Сбрасываем состояние, если клик вне кнопки
    }
    return (was_pressed != button->is_pressed) ? 1 : 0; // Вернуть 1, если состояние изменилось
}

void button_render(const ButtonState* button, Renderer* renderer) {
    if (!button || !renderer) {
        return;
    }

    // 1. Определяем цвет в зависимости от состояния
    uint32_t bg_color = MENU_BUTTON_BACKGROUND_COLOR_DEFAULT; // Светло-серый по умолчанию
    uint32_t text_color = MENU_BUTTON_TEXT_COLOR; // Черный текст
    if (button->is_pressed) {
        bg_color = MENU_BUTTON_BACKGROUND_COLOR_PRESSED; // Темнее при нажатии
    } else if (button->is_hovered) {
        bg_color = MENU_BUTTON_BACKGROUND_COLOR_HOVER; // Светлее при наведении (если будет реализовано)
    }

    // 2. Отрисовка фона кнопки
    renderer_draw_quad(renderer, button->x, button->y, button->width, button->height, bg_color);

    // 3. Отрисовка рамки кнопки
    renderer_draw_quad(renderer, button->x, button->y, button->width, button->height, MENU_BUTTON_BORDER_COLOR); // Серая рамка

    // 4. Отрисовка текста по центру кнопки
    if (button->label && strlen(button->label) > 0) {
        // TODO: Рассчитать позицию для центрирования текста
        // Пока просто рисуем в центре по X и немного ниже центра по Y
        float text_x = button->x + button->width / 2 - strlen(button->label) * 4.0f; // Грубая оценка ширины
        float text_y = button->y + button->height / 2 + 5.0f;
        renderer_draw_text(renderer, button->label, text_x, text_y, 1.0f, text_color, button->width);
    }
}
