// src/gui/widgets.h
// Этот файл содержит определения и интерфейсы для виджетов GUI, таких как текстовое поле и кнопка.

#ifndef SEE_CODE_WIDGETS_H
#define SEE_CODE_WIDGETS_H

#include <stdint.h>
#include <stddef.h>

// --- Текстовое поле ввода ---

// Состояние текстового поля ввода
typedef struct {
    char* buffer;           // Динамический буфер для хранения текста
    size_t buffer_size;     // Общий размер выделенной памяти для буфера
    size_t text_length;     // Текущая длина текста в буфере (без учета '\0')
    size_t cursor_pos;      // Позиция курсора в байтах от начала буфера
    int is_focused;         // Флаг, находится ли поле ввода в фокусе
    float x, y, width, height; // Позиция и размеры поля на экране
    int multiline;          // Флаг поддержки многострочного ввода (1 = да)
    // Добавить прокрутку, если текст не помещается?
} TextInputState;

// Инициализация состояния текстового поля
// Возвращает 1 при успехе, 0 при ошибке
int text_input_init(TextInputState* input, float x, float y, float width, float height);

// Очистка состояния текстового поля (освобождение памяти)
void text_input_destroy(TextInputState* input);

// Обработка нажатия клавиши
// key_code: ASCII код символа или код специальной клавиши (например, '\b' для Backspace)
// Возвращает 1, если состояние изменилось и нужна перерисовка
int text_input_handle_key(TextInputState* input, int key_code);

// Обработка клика мыши/тачскрина
// mouse_x, mouse_y: координаты клика
// Возвращает 1, если фокус изменился
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
void text_input_render(const TextInputState* input, void* renderer); // void* для избежания циклических зависимостей

// --- Кнопка ---

// Состояние кнопки
typedef struct {
    float x, y, width, height; // Позиция и размеры кнопки
    char* label;               // Текст на кнопке
    int is_pressed;            // Флаг, нажата ли кнопка в данный момент
    int is_hovered;            // Флаг, находится ли курсор мыши над кнопкой (для будущего использования)
    // Можно добавить callback-функцию или ID для обработки нажатия
} ButtonState;

// Инициализация состояния кнопки
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
void button_render(const ButtonState* button, void* renderer); // void* для избежания циклических зависимостей

#endif // SEE_CODE_WIDGETS_H
