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
    GLuint shader_program;
    GLuint vbo;
    GLint pos_attrib;
    GLint color_attrib;
    GLint mvp_uniform;
    // --- Конец добавления ---
};

// --- Добавлено для рендеринга текста ---
// Шейдер для рисования цветных примитивов
static const char* vertex_shader_source =
    "attribute vec2 position;\n"
    "attribute vec4 color;\n"
    "varying vec4 frag_color;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "  gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
    "  frag_color = color;\n"
    "}\n";

static const char* fragment_shader_source =
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

    // Create EGL surface
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

    // --- Инициализация ресурсов для рендеринга примитивов ---
    renderer->shader_program = create_program(vertex_shader_source, fragment_shader_source);
    if (!renderer->shader_program) {
        log_error("Failed to create shader program for primitives");
        // Продолжаем, но рендеринг текста/примитивов не будет работать
    } else {
        renderer->pos_attrib = glGetAttribLocation(renderer->shader_program, "position");
        renderer->color_attrib = glGetAttribLocation(renderer->shader_program, "color");
        renderer->mvp_uniform = glGetUniformLocation(renderer->shader_program, "mvp");

        glGenBuffers(1, &renderer->vbo);
    }
    // --- Конец инициализации ресурсов ---

    log_info("Renderer initialized with GLES2 context");
    return renderer;
}

void renderer_destroy(Renderer* renderer) {
    if (!renderer) {
        return;
    }
    // --- Освобождение ресурсов для рендеринга ---
    if (renderer->shader_program) glDeleteProgram(renderer->shader_program);
    if (renderer->vbo) glDeleteBuffers(1, &renderer->vbo);
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
    glViewport(0, 0, renderer->width, renderer->height);
    return 1;
}

int renderer_end_frame(Renderer* renderer) {
    if (!renderer) {
        return 0;
    }
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
    glViewport(0, 0, width, height);
}

void renderer_clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_draw_quad(float x, float y, float width, float height,
                       float r, float g, float b, float a) {
    if (!renderer || !renderer->shader_program) {
         // Fallback если шейдер не скомпилировался
         log_debug("Falling back to glColor for quad");
         glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black background for fallback
         glClear(GL_COLOR_BUFFER_BIT);
         glColor4f(r, g, b, a);
         GLfloat vertices[] = { x, y, x + width, y, x, y + height, x + width, y + height };
         glEnableVertexAttribArray(0);
         glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
         glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
         glDisableVertexAttribArray(0);
         glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // Reset color
         return;
    }

    glUseProgram(renderer->shader_program);

    // Матрица ортографической проекции
    float mvp[16] = {
        2.0f / renderer->width, 0, 0, 0,
        0, -2.0f / renderer->height, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1
    };
    glUniformMatrix4fv(renderer->mvp_uniform, 1, GL_FALSE, mvp);

    GLfloat vertices[] = {
        x, y,
        x + width, y,
        x, y + height,
        x + width, y + height
    };
    GLfloat colors[] = {
        r, g, b, a,
        r, g, b, a,
        r, g, b, a,
        r, g, b, a
    };

    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) + sizeof(colors), NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertices), sizeof(colors), colors);

    glVertexAttribPointer(renderer->pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(renderer->pos_attrib);
    glVertexAttribPointer(renderer->color_attrib, 4, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid*)sizeof(vertices));
    glEnableVertexAttribArray(renderer->color_attrib);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(renderer->pos_attrib);
    glDisableVertexAttribArray(renderer->color_attrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

// --- РЕАЛИЗАЦИЯ ФУНКЦИИ РЕНДЕРИНГА ТЕКСТА ---
void renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text || !renderer->shader_program) {
        // Fallback если шейдер не скомпилировался или текст пустой
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

    glUseProgram(renderer->shader_program);

    // Матрица ортографической проекции
    float mvp[16] = {
        2.0f / renderer->width, 0, 0, 0,
        0, -2.0f / renderer->height, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1
    };
    glUniformMatrix4fv(renderer->mvp_uniform, 1, GL_FALSE, mvp);

    size_t len = strlen(text);
    if (len == 0) {
        glUseProgram(0);
        return;
    }

    // Резервируем память для вершин и цветов
    const int VERTS_PER_CHAR = 6;
    const int FLOATS_PER_VERT = 2;
    const int FLOATS_PER_COLOR = 4;
    const float char_width = 8.0f * scale;
    const float char_height = 16.0f * scale;

    GLfloat *vertices = malloc(len * VERTS_PER_CHAR * FLOATS_PER_VERT * sizeof(GLfloat));
    GLfloat *colors = malloc(len * VERTS_PER_CHAR * FLOATS_PER_COLOR * sizeof(GLfloat));

    if (!vertices || !colors) {
        log_error("Failed to allocate memory for text rendering buffers");
        free(vertices);
        free(colors);
        glUseProgram(0);
        return;
    }

    float cursor_x = x;
    float cursor_y = y;
    int vert_index = 0;
    int color_index = 0;

    // Цвет текста
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;

    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c == '\n') {
            cursor_x = x;
            cursor_y += char_height;
            continue;
        }

        // Создаем прямоугольник для символа (простая визуализация)
        float x1 = cursor_x;
        float y1 = cursor_y;
        float x2 = cursor_x + char_width;
        float y2 = cursor_y + char_height;

        // Вершины прямоугольника (2 треугольника)
        GLfloat char_verts[] = {
            x1, y1, x2, y1, x1, y2, // Первый треугольник
            x2, y1, x2, y2, x1, y2  // Второй треугольник
        };

        // Копируем вершины
        memcpy(&vertices[vert_index], char_verts, sizeof(char_verts));
        vert_index += VERTS_PER_CHAR * FLOATS_PER_VERT;

        // Заполняем цвета для всех 6 вершин
        for (int j = 0; j < VERTS_PER_CHAR; j++) {
            colors[color_index++] = r;
            colors[color_index++] = g;
            colors[color_index++] = b;
            colors[color_index++] = a;
        }

        cursor_x += char_width;
    }

    int total_vertices = vert_index / FLOATS_PER_VERT;

    // Передаем данные в буфер OpenGL
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 total_vertices * (FLOATS_PER_VERT + FLOATS_PER_COLOR) * sizeof(GLfloat),
                 NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, total_vertices * FLOATS_PER_VERT * sizeof(GLfloat), vertices);
    glBufferSubData(GL_ARRAY_BUFFER,
                    total_vertices * FLOATS_PER_VERT * sizeof(GLfloat),
                    total_vertices * FLOATS_PER_COLOR * sizeof(GLfloat),
                    colors);

    // Атрибуты вершин
    glVertexAttribPointer(renderer->pos_attrib, FLOATS_PER_VERT, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(renderer->pos_attrib);
    glVertexAttribPointer(renderer->color_attrib, FLOATS_PER_COLOR, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid*)(total_vertices * FLOATS_PER_VERT * sizeof(GLfloat)));
    glEnableVertexAttribArray(renderer->color_attrib);

    // Рисуем элементы
    glDrawArrays(GL_TRIANGLES, 0, total_vertices);

    // Очищаем состояние
    glDisableVertexAttribArray(renderer->pos_attrib);
    glDisableVertexAttribArray(renderer->color_attrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    free(vertices);
    free(colors);
}
// --- КОНЕЦ РЕАЛИЗАЦИИ ФУНКЦИИ РЕНДЕРИНГА ТЕКСТА ---
