// src/gui/renderer.c
#include "see_code/gui/renderer.h"
#include "see_code/gui/renderer/gl_context.h"
#include "see_code/gui/renderer/gl_shaders.h"
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>

#define MAX_VERTICES 16384 // 16384 вершин = 4096 квадратов на батч

// Структура одной вершины для батчинга
typedef struct {
    float x, y;         // Позиция
    float u, v;         // Текстурные координаты
    uint32_t color;     // Цвет в формате 0xAABBGGRR
} BatchVertex;

struct Renderer {
    GLContext* gl_ctx;
    int width;
    int height;
    
    // Шейдеры
    GLuint solid_shader;
    GLuint textured_shader;

    // MVP матрица
    float mvp[16];

    // VBO и буфер для батчинга
    GLuint vbo;
    BatchVertex* vertices;
    int vertex_count;

    // Состояние рендеринга
    GLuint current_texture;

    // Указатель на данные рендерера текста
    void* text_internal_data_private;
};

// --- Внутренние функции ---

// Отправляет текущий батч на отрисовку
static void renderer_flush_internal(Renderer* renderer) {
    if (renderer->vertex_count == 0) {
        return;
    }

    // Выбираем шейдер в зависимости от того, используется ли текстура
    GLuint shader = (renderer->current_texture != 0) ? renderer->textured_shader : renderer->solid_shader;
    glUseProgram(shader);
    
    // Биндим текстуру, если нужно
    if (renderer->current_texture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->current_texture);
        glUniform1i(glGetUniformLocation(shader, "u_texture"), 0);
    }

    // Загружаем MVP-матрицу
    glUniformMatrix4fv(glGetUniformLocation(shader, "u_mvp"), 1, GL_FALSE, renderer->mvp);
    
    // Биндим VBO и загружаем данные
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, renderer->vertex_count * sizeof(BatchVertex), renderer->vertices, GL_DYNAMIC_DRAW);
    
    // Настраиваем атрибуты вершин
    GLint pos_attrib = glGetAttribLocation(shader, "a_position");
    GLint tex_attrib = glGetAttribLocation(shader, "a_texcoord");
    GLint col_attrib = glGetAttribLocation(shader, "a_color");
    
    glEnableVertexAttribArray(pos_attrib);
    glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void*)offsetof(BatchVertex, x));
    
    glEnableVertexAttribArray(tex_attrib);
    glVertexAttribPointer(tex_attrib, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void*)offsetof(BatchVertex, u));

    glEnableVertexAttribArray(col_attrib);
    glVertexAttribPointer(col_attrib, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(BatchVertex), (void*)offsetof(BatchVertex, color));

    // Отрисовываем!
    glDrawArrays(GL_TRIANGLES, 0, renderer->vertex_count);

    // Сбрасываем счетчик вершин
    renderer->vertex_count = 0;
}

// Обновляет MVP матрицу
static void renderer_update_mvp(Renderer* renderer) {
    float w = (float)renderer->width;
    float h = (float)renderer->height;
    // Простая ортографическая проекция
    memset(renderer->mvp, 0, sizeof(renderer->mvp));
    renderer->mvp[0] = 2.0f / w;
    renderer->mvp[5] = -2.0f / h;
    renderer->mvp[10] = 1.0f;
    renderer->mvp[12] = -1.0f;
    renderer->mvp[13] = 1.0f;
    renderer->mvp[15] = 1.0f;
}

// --- Публичные функции ---

Renderer* renderer_create(int width, int height) {
    Renderer* renderer = calloc(1, sizeof(Renderer));
    if (!renderer) {
        log_error("Failed to allocate memory for Renderer");
        return NULL;
    }

    renderer->width = width;
    renderer->height = height;

    renderer->gl_ctx = gl_context_create(width, height);
    if (!renderer->gl_ctx) {
        renderer_destroy(renderer);
        return NULL;
    }

    // Компилируем шейдеры
    renderer->solid_shader = gl_shaders_create_program_from_sources(gl_shaders_batch_vertex_shader_source, gl_shaders_batch_solid_fragment_source);
    renderer->textured_shader = gl_shaders_create_program_from_sources(gl_shaders_batch_vertex_shader_source, gl_shaders_batch_textured_fragment_source);

    if (!renderer->solid_shader || !renderer->textured_shader) {
        log_error("Failed to create shader programs");
        renderer_destroy(renderer);
        return NULL;
    }
    
    // Выделяем память для вершин
    renderer->vertices = malloc(MAX_VERTICES * sizeof(BatchVertex));
    glGenBuffers(1, &renderer->vbo);

    // Инициализируем рендерер текста
    if (!text_renderer_init(renderer, NULL)) {
        log_error("Failed to initialize text renderer");
        renderer_destroy(renderer);
        return NULL;
    }

    renderer_update_mvp(renderer);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    log_info("Batch renderer created successfully");
    return renderer;
}

void renderer_destroy(Renderer* renderer) {
    if (!renderer) return;
    
    text_renderer_cleanup(renderer);
    
    if (renderer->solid_shader) glDeleteProgram(renderer->solid_shader);
    if (renderer->textured_shader) glDeleteProgram(renderer->textured_shader);
    if (renderer->vbo) glDeleteBuffers(1, &renderer->vbo);
    if (renderer->gl_ctx) gl_context_destroy(renderer->gl_ctx);
    
    free(renderer->vertices);
    free(renderer);
}

void renderer_resize(Renderer* renderer, int width, int height) {
    renderer->width = width;
    renderer->height = height;
    if (renderer->gl_ctx) {
        gl_context_resize(renderer->gl_ctx, width, height);
    }
    renderer_update_mvp(renderer);
}

void renderer_clear(Renderer* renderer, float r, float g, float b, float a) {
    gl_context_clear(renderer->gl_ctx, r, g, b, a);
}

void renderer_begin_frame(Renderer* renderer) {
    renderer_flush_internal(renderer); // Сбрасываем батч с предыдущего кадра, если он есть
    renderer->vertex_count = 0;
    renderer->current_texture = 0;
}

void renderer_flush(Renderer* renderer) {
    renderer_flush_internal(renderer);
}

int renderer_end_frame(Renderer* renderer) {
    renderer_flush_internal(renderer); // Финальный сброс перед показом кадра
    return gl_context_end_frame(renderer->gl_ctx);
}

// Добавляет 6 вершин (2 треугольника) для квадрата
void add_quad_to_batch(Renderer* renderer, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color, GLuint texture_id) {
    if (renderer->vertex_count + 6 > MAX_VERTICES || (renderer->current_texture != texture_id && renderer->vertex_count > 0)) {
        renderer_flush_internal(renderer);
    }
    renderer->current_texture = texture_id;

    // Конвертируем цвет из 0xAARRGGBB в 0xAABBGGRR (для OpenGL little-endian)
    uint32_t final_color = (color & 0xFF00FF00) | ((color >> 16) & 0xFF) | ((color & 0xFF) << 16);

    BatchVertex* v = renderer->vertices + renderer->vertex_count;
    // Треугольник 1
    v[0] = (BatchVertex){x,     y,     u0, v0, final_color};
    v[1] = (BatchVertex){x + w, y,     u1, v0, final_color};
    v[2] = (BatchVertex){x,     y + h, u0, v1, final_color};
    // Треугольник 2
    v[3] = (BatchVertex){x + w, y,     u1, v0, final_color};
    v[4] = (BatchVertex){x,     y + h, u0, v1, final_color};
    v[5] = (BatchVertex){x + w, y + h, u1, v1, final_color};
    
    renderer->vertex_count += 6;
}

void renderer_draw_quad(Renderer* renderer, float x, float y, float width, float height, uint32_t color) {
    // Для сплошного квадрата используем текстурные координаты (0,0) и текстуру с ID 0
    add_quad_to_batch(renderer, x, y, width, height, 0.0f, 0.0f, 0.0f, 0.0f, color, 0);
}

void renderer_draw_textured_quad(Renderer* renderer, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color) {
    GLuint texture_id = renderer_get_font_atlas_texture(renderer);
    add_quad_to_batch(renderer, x, y, w, h, u0, v0, u1, v1, color, texture_id);
}

void renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, uint32_t color, float max_width) {
    text_renderer_draw_text(renderer, text, x, y, scale, color, max_width);
}

int renderer_get_width(const Renderer* renderer) {
    return renderer ? renderer->width : 0;
}

int renderer_get_height(const Renderer* renderer) {
    return renderer ? renderer->height : 0;
}
