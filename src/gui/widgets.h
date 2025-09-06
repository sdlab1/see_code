// src/gui/widgets.h
#ifndef SEE_CODE_WIDGETS_H
#define SEE_CODE_WIDGETS_H

#include <stddef.h>
#include <stdint.h>

// Forward declarations
struct Renderer;
typedef struct Renderer Renderer;

// --- TextInputState ---

typedef struct {
    char* buffer;           // Динамический буфер для хранения текста
    size_t buffer_size;     // Общий размер выделенной памяти для буфера
    size_t text_length;     // Текущая длина текста в буфере (без учета '\0')
    size_t cursor_pos;      // Позиция курсора в байтах от начала буфера
    int is_focused;         // Флаг, находится ли поле ввода в фокусе
    float x, y, width, height; // Позиция и размеры поля на экране
    int multiline;          // Флаг поддержки многострочного ввода (1 = да)
    // TODO: Добавить прокрутку, если текст не помещается?
    // TODO: Добавить поддержку UTF-8 для более точного перемещения курсора?
} TextInputState;

// Инициализация состояния текстового поля
// x, y, width, height: позиция и размеры
// Возвращает 1 при успехе, 0 при ошибке
int text_input_init(TextInputState* input, float x, float y, float width, float height);

// Очистка состояния текстового поля (освобождение памяти)
void text_input_destroy(TextInputState* input);

// Обработка нажатия клавиши
// key_code: ASCII код символа или код специальной клавиши
//             '\b' (8)   - Backspace
//             127        - Delete
//             '\n' (10)  - Enter/Line Feed
//             '\r' (13)  - Carriage Return
//             27         - Escape
//             0x10000    - Влево (пользовательский код)
//             0x10001    - Вправо (пользовательский код)
//             0x10002    - Вверх (пользовательский код)
//             0x10003    - Вниз (пользовательский код)
// Возвращает 1, если состояние изменилось и нужна перерисовка
int text_input_handle_key(TextInputState* input, int key_code);

// Обработка клика мыши/тачскрина
// mouse_x, mouse_y: координаты клика
// Возвращает 1, если фокус изменился или нужно перерисовать
int text_input_handle_click(TextInputState* input, float mouse_x, float mouse_y);

// Получение текста из поля ввода (для использования другими частями программы)
// Возвращаемый указатель действителен до следующего вызова text_input_* или text_input_destroy
const char* text_input_get_text(const TextInputState* input);

// Получение длины текста
size_t text_input_get_length(const TextInputState* input);

// Установка фокуса на поле ввода
void text_input_set_focus(TextInputState* input, int focused);

// Рендеринг текстового поля
// renderer: указатель на инициализированный рендерер
void text_input_render(const TextInputState* input, Renderer* renderer);


// --- ButtonState ---

typedef struct {
    float x, y, width, height; // Позиция и размеры кнопки
    char* label;               // Текст на кнопке (динамически выделенная память)
    int is_pressed;            // Флаг, нажата ли кнопка в данный момент
    int is_hovered;            // Флаг, находится ли курсор мыши над кнопкой
    // TODO: Можно добавить callback-функцию или ID для обработки нажатия
} ButtonState;

// Инициализация состояния кнопки
// x, y, width, height: позиция и размеры
// label: текст на кнопке (будет скопирован)
// Возвращает 1 при успехе, 0 при ошибке
int button_init(ButtonState* button, float x, float y, float width, float height, const char* label);

// Очистка состояния кнопки
void button_destroy(ButtonState* button);

// Обработка клика мыши/тачскрина
// mouse_x, mouse_y: координаты клика
// Возвращает 1, если кнопка была нажата (клик внутри области кнопки)
int button_handle_click(ButtonState* button, float mouse_x, float mouse_y);

// Рендеринг кнопки
// renderer: указатель на инициализированный рендерер
void button_render(const ButtonState* button, Renderer* renderer);

#endif // SEE_CODE_WIDGETS_H
