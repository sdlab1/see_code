// src/gui/renderer.c
#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <math.h> // Для ceil

struct Renderer {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int width;
    int height;
    // --- Добавлено для рендеринга текста ---
    GLuint simple_shader_program;
    GLuint simple_vbo;
    GLuint simple_ebo; // Для индексов
    GLint simple_pos_attrib;
    GLint simple_color_attrib;
    GLint simple_mvp_uniform;
    // --- Конец добавления ---
};

// --- Добавлено для рендеринга текста ---
// Очень простой шейдер для рисования примитивов (квадратов, линий)
static const char* simple_vertex_shader_source =
    "attribute vec2 position;\n"
    "attribute vec4 color;\n"
    "varying vec4 frag_color;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "  gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
    "  frag_color = color;\n"
    "}\n";

static const char* simple_fragment_shader_source =
    "precision mediump float;\n"
    "varying vec4 frag_color;\n"
    "void main() {\n"
    "  gl_FragColor = frag_color;\n"
    "}\n";

// --- Вспомогательные функции для шейдеров ---
static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char* info_log = malloc(info_len);
            glGetShaderInfoLog(shader, info_len, NULL, info_log);
            log_error("Shader compilation failed: %s", info_log);
            free(info_log);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint create_program(const char* vertex_source, const char* fragment_source) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (!vertex_shader) return 0;

    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!fragment_shader) {
        glDeleteShader(vertex_shader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char* info_log = malloc(info_len);
            glGetProgramInfoLog(program, info_len, NULL, info_log);
            log_error("Program linking failed: %s", info_log);
            free(info_log);
        }
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}
// --- Конец вспомогательных функций ---

Renderer* renderer_create(int width, int height) {
    Renderer* renderer = malloc(sizeof(Renderer));
    if (!renderer) {
        log_error("Failed to allocate memory for Renderer");
        return NULL;
    }
    memset(renderer, 0, sizeof(Renderer));
    renderer->width = width;
    renderer->height = height;

    // Initialize EGL
    renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (renderer->display == EGL_NO_DISPLAY) {
        log_error("Failed to get EGL display");
        free(renderer);
        return NULL;
    }
    if (!eglInitialize(renderer->display, NULL, NULL)) {
        log_error("Failed to initialize EGL");
        free(renderer);
        return NULL;
    }

    // Choose EGL config
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(renderer->display, config_attribs, &config, 1, &num_configs) || num_configs == 0) {
        log_error("Failed to choose EGL config");
        eglTerminate(renderer->display);
        free(renderer);
        return NULL;
    }

    // Create EGL surface (this would typically be provided by Termux:GUI)
    renderer->surface = eglCreateWindowSurface(renderer->display, config, NULL, NULL);
    if (renderer->surface == EGL_NO_SURFACE) {
        log_error("Failed to create EGL surface");
        eglTerminate(renderer->display);
        free(renderer);
        return NULL;
    }

    // Create EGL context
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    renderer->context = eglCreateContext(renderer->display, config, EGL_NO_CONTEXT, context_attribs);
    if (renderer->context == EGL_NO_CONTEXT) {
        log_error("Failed to create EGL context");
        eglDestroySurface(renderer->display, renderer->surface);
        eglTerminate(renderer->display);
        free(renderer);
        return NULL;
    }

    // Make context current
    if (!eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context)) {
        log_error("Failed to make EGL context current");
        eglDestroyContext(renderer->display, renderer->context);
        eglDestroySurface(renderer->display, renderer->surface);
        eglTerminate(renderer->display);
        free(renderer);
        return NULL;
    }

    // --- Инициализация ресурсов для рендеринга текста ---
    renderer->simple_shader_program = create_program(simple_vertex_shader_source, simple_fragment_shader_source);
    if (!renderer->simple_shader_program) {
        log_error("Failed to create simple shader program");
        // Продолжаем, но рендеринг текста не будет работать
    } else {
        renderer->simple_pos_attrib = glGetAttribLocation(renderer->simple_shader_program, "position");
        renderer->simple_color_attrib = glGetAttribLocation(renderer->simple_shader_program, "color");
        renderer->simple_mvp_uniform = glGetUniformLocation(renderer->simple_shader_program, "mvp");

        // Создаем VBO и EBO
        glGenBuffers(1, &renderer->simple_vbo);
        glGenBuffers(1, &renderer->simple_ebo);
    }
    // --- Конец инициализации ресурсов для рендеринга текста ---

    log_info("Renderer initialized with GLES2 context");
    return renderer;
}

void renderer_destroy(Renderer* renderer) {
    if (!renderer) {
        return;
    }
    // --- Освобождение ресурсов для рендеринга текста ---
    if (renderer->simple_shader_program) glDeleteProgram(renderer->simple_shader_program);
    if (renderer->simple_vbo) glDeleteBuffers(1, &renderer->simple_vbo);
    if (renderer->simple_ebo) glDeleteBuffers(1, &renderer->simple_ebo);
    // --- Конец освобождения ресурсов ---

    if (renderer->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (renderer->context != EGL_NO_CONTEXT) {
            eglDestroyContext(renderer->display, renderer->context);
        }
        if (renderer->surface != EGL_NO_SURFACE) {
            eglDestroySurface(renderer->display, renderer->surface);
        }
        eglTerminate(renderer->display);
    }
    free(renderer);
}

int renderer_begin_frame(Renderer* renderer) {
    if (!renderer) {
        return 0;
    }
    // Set viewport
    glViewport(0, 0, renderer->width, renderer->height);
    return 1;
}

int renderer_end_frame(Renderer* renderer) {
    if (!renderer) {
        return 0;
    }
    // Swap buffers
    if (!eglSwapBuffers(renderer->display, renderer->surface)) {
        log_error("Failed to swap EGL buffers");
        return 0;
    }
    return 1;
}

void renderer_resize(Renderer* renderer, int width, int height) {
    if (!renderer) {
        return;
    }
    renderer->width = width;
    renderer->height = height;
    // Update viewport
    glViewport(0, 0, width, height);
}

void renderer_clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_draw_quad(float x, float y, float width, float height,
                       float r, float g, float b, float a) {
    // Simple quad rendering using GLES2
    GLfloat vertices[] = {
        x, y,
        x + width, y,
        x, y + height,
        x + width, y + height
    };
    // Set color
    glColor4f(r, g, b, a);
    // Draw quad
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
}

// --- РЕАЛИЗАЦИЯ ФУНКЦИИ РЕНДЕРИНГА ТЕКСТА ---
void renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text || !renderer->simple_shader_program) {
        // Если шейдер не скомпилировался, рисуем заглушку
        if (renderer && text) {
             log_debug("Falling back to placeholder for text: %.20s", text);
             float text_width = strlen(text) * 8.0f * scale;
             float text_height = 16.0f * scale;
             float r = ((color >> 16) & 0xFF) / 255.0f;
             float g = ((color >> 8) & 0xFF) / 255.0f;
             float b = (color & 0xFF) / 255.0f;
             float a = ((color >> 24) & 0xFF) / 255.0f;
             renderer_draw_quad(x, y, text_width, text_height, r, g, b, a);
        }
        return;
    }

    // Активируем шейдерную программу
    glUseProgram(renderer->simple_shader_program);

    // Матрица ортографической проекции (упрощенная)
    // OpenGL ES координаты: (-1, -1) - (1, 1)
    // Нам нужно преобразовать пиксельные координаты в эти
    float mvp[16] = {
        2.0f / renderer->width, 0, 0, 0,
        0, -2.0f / renderer->height, 0, 0, // Инвертируем Y
        0, 0, 1, 0,
        -1, 1, 0, 1 // Перенос
    };
    glUniformMatrix4fv(renderer->simple_mvp_uniform, 1, GL_FALSE, mvp);

    // Цвет текста
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;

    size_t len = strlen(text);
    if (len == 0) return;

    // Резервируем память для вершин и цветов
    // Каждый символ как прямоугольник из 2 треугольников = 6 вершин * 2 координаты = 12 float
    // + 6 вершин * 4 цвета = 24 float
    // И 6 индексов на символ
    const int VERTS_PER_CHAR = 6; // 2 треугольника
    const int FLOATS_PER_VERT = 2; // x, y
    const int FLOATS_PER_COLOR = 4; // r, g, b, a
    const int INDICES_PER_CHAR = 6; // 2 треугольника

    GLfloat *vertices = malloc(len * VERTS_PER_CHAR * FLOATS_PER_VERT * sizeof(GLfloat));
    GLfloat *colors = malloc(len * VERTS_PER_CHAR * FLOATS_PER_COLOR * sizeof(GLfloat));
    GLushort *indices = malloc(len * INDICES_PER_CHAR * sizeof(GLushort));

    if (!vertices || !colors || !indices) {
        log_error("Failed to allocate memory for text rendering buffers");
        free(vertices);
        free(colors);
        free(indices);
        glUseProgram(0);
        return;
    }

    float cursor_x = x;
    float cursor_y = y;
    const float char_width = 8.0f * scale;
    const float char_height = 16.0f * scale;

    int vert_index = 0;
    int color_index = 0;
    int index_index = 0;
    int element_count = 0; // Общее количество вершин для отрисовки

    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c == '\n') {
            cursor_x = x;
            cursor_y += char_height;
            continue;
        }

        // Создаем прямоугольник для символа
        float x1 = cursor_x;
        float y1 = cursor_y;
        float x2 = cursor_x + char_width;
        float y2 = cursor_y + char_height;

        // Вершины прямоугольника (2 треугольника)
        GLfloat char_verts[] = {
            x1, y1, // 0
            x2, y1, // 1
            x1, y2, // 2
            x2, y1, // 1 (повтор)
            x2, y2, // 3
            x1, y2  // 2 (повтор)
        };

        // Цвета для каждой вершины
        for (int j = 0; j < VERTS_PER_CHAR; j++) {
            colors[color_index++] = r;
            colors[color_index++] = g;
            colors[color_index++] = b;
            colors[color_index++] = a;
        }

        // Индексы (просто 0, 1, 2, 3, 4, 5 для этого символа)
        for (int j = 0; j < INDICES_PER_CHAR; j++) {
            indices[index_index++] = element_count + j;
        }
        element_count += VERTS_PER_CHAR;

        // Копируем вершины
        memcpy(&vertices[vert_index], char_verts, sizeof(char_verts));
        vert_index += VERTS_PER_CHAR * FLOATS_PER_VERT;

        cursor_x += char_width;
    }

    // Передаем данные в буферы OpenGL
    glBindBuffer(GL_ARRAY_BUFFER, renderer->simple_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 element_count * (FLOATS_PER_VERT + FLOATS_PER_COLOR) * sizeof(GLfloat),
                 NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, element_count * FLOATS_PER_VERT * sizeof(GLfloat), vertices);
    glBufferSubData(GL_ARRAY_BUFFER,
                    element_count * FLOATS_PER_VERT * sizeof(GLfloat),
                    element_count * FLOATS_PER_COLOR * sizeof(GLfloat),
                    colors);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->simple_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (index_index) * sizeof(GLushort), indices, GL_STATIC_DRAW);

    // Атрибуты вершин
    glVertexAttribPointer(renderer->simple_pos_attrib, FLOATS_PER_VERT, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(renderer->simple_pos_attrib);

    glVertexAttribPointer(renderer->simple_color_attrib, FLOATS_PER_COLOR, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid*)(element_count * FLOATS_PER_VERT * sizeof(GLfloat)));
    glEnableVertexAttribArray(renderer->simple_color_attrib);

    // Рисуем элементы
    glDrawElements(GL_TRIANGLES, index_index, GL_UNSIGNED_SHORT, 0);

    // Очищаем состояние
    glDisableVertexAttribArray(renderer->simple_pos_attrib);
    glDisableVertexAttribArray(renderer->simple_color_attrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);

    free(vertices);
    free(colors);
    free(indices);
}
// --- КОНЕЦ РЕАЛИЗАЦИИ ФУНКЦИИ РЕНДЕРИНГА ТЕКСТА ---
