// src/gui/renderer/text_renderer.h
#ifndef SEE_CODE_TEXT_RENDERER_H
#define SEE_CODE_TEXT_RENDERER_H

#include <GLES2/gl2.h> // Для GLuint и т.д.
#include <stddef.h>    // Для size_t

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct Renderer Renderer; // From parent renderer.h

// --- Внутренние структуры данных для text_renderer ---

// Кэш для одного глифа
struct glyph_cache_entry {
    int is_loaded;
    float u0, v0, u1, v1; // Нормализованные UV координаты в атласе
    int width, height;     // Размеры глифа в пикселях
    int bearing_x, bearing_y; // Смещение от базовой точки до верхнего левого угла глифа
    int advance_x;         // Горизонтальное продвижение курсора после этого глифа (в пикселях)
};

// Внутренние данные рендерера текста
struct TextRendererInternalData {
    int is_freetype_initialized;
    // FreeType
    FT_Library ft_library;
    FT_Face ft_face;
    // Атлас текстур
    GLuint texture_atlas_id;
    unsigned char* texture_atlas_data;
    int atlas_width;
    int atlas_height;
    // Кэш глифов для печатаемых ASCII символов (32-126)
    struct glyph_cache_entry glyph_cache[96];
    // Шейдеры и VBO для текстурированного рендеринга
    GLuint shader_program_textured;
    GLuint vbo_textured; // Новый VBO для текстурированных квадратов
};

// --- Основные функции text_renderer ---

/**
 * @brief Инициализирует рендерер текста с использованием FreeType.
 *
 * Эта функция пытается инициализировать библиотеку FreeType, загрузить шрифт
 * (используя font_path_hint или пути по умолчанию из config.h),
 * создать атлас текстур, скомпилировать шейдеры и создать VBO.
 *
 * @param renderer Указатель на основной рендерер. Внутренние данные будут
 *                 сохранены в renderer->text_internal_data_private.
 * @param font_path_hint Предпочтительный путь к .ttf файлу. Может быть NULL.
 * @return 1 при успешной инициализации, 0 при ошибке.
 */
int text_renderer_init(Renderer* renderer, const char* font_path_hint);

/**
 * @brief Очищает ресурсы, выделенные text_renderer_init.
 *
 * Освобождает память, удаляет текстуры, буферы, шейдеры и завершает работу FreeType.
 *
 * @param renderer Указатель на основной рендерер.
 */
void text_renderer_cleanup(Renderer* renderer);

/**
 * @brief Рисует текст на экране с использованием FreeType.
 *
 * Если инициализация FreeType прошла успешно, рисует текст.
 * В противном случае рисует placeholder-прямоугольник.
 *
 * @param renderer Указатель на основной рендерер.
 * @param text Строка UTF-8 для рендеринга (пока обрабатываются только ASCII 32-126).
 * @param x Координата X левого края текста (пиксели от левого края экрана).
 * @param y Координата Y базовой линии текста (пиксели от верхнего края экрана).
 * @param scale Коэффициент масштабирования текста.
 * @param color Цвет в формате 0xAARRGGBB.
 */
void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color);

// --- Вспомогательные функции (используются внутри модуля) ---

/**
 * @brief Загружает глиф в атлас текстур и кэширует его данные.
 *
 * @param tr_data Указатель на внутренние данные рендерера текста.
 * @param char_code Код символа (предполагается ASCII 32-126).
 * @return 1 при успешной загрузке или если глиф уже был загружен, 0 при ошибке.
 */
int load_glyph_into_atlas_internal(struct TextRendererInternalData* tr_data, unsigned long char_code);

/**
 * @brief Инициализирует атлас текстур и кэш глифов.
 *
 * @param tr_data Указатель на внутренние данные рендерера текста.
 * @return 1 при успешной инициализации, 0 при ошибке.
 */
int text_renderer_init_atlas(struct TextRendererInternalData* tr_data);

/**
 * @brief Очищает ресурсы атласа текстур и кэша глифов.
 *
 * @param tr_data Указатель на внутренние данные рендерера текста.
 */
void text_renderer_cleanup_atlas(struct TextRendererInternalData* tr_data);


#ifdef __cplusplus
}
#endif

#endif // SEE_CODE_TEXT_RENDERER_H
