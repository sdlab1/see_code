// src/gui/renderer.c
#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // Для путей к шрифтам
#include <stdlib.h>
#include <string.h>
#include <math.h> // Для ceil

struct Renderer {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int width;
    int height;
    // --- Добавлено для рендеринга примитивов ---
    GLuint shader_program;
    GLuint vbo;
    GLint pos_attrib;
    GLint color_attrib;
    GLint mvp_uniform;
    // --- Конец добавления ---
    // --- Добавлено для FreeType ---
    FT_Library ft_library;
    FT_Face ft_face;
    GLuint texture_atlas_id;
    unsigned char* texture_atlas_data;
    int atlas_width;
    int atlas_height;
    int is_freetype_initialized;
    // --- Конец добавления ---
};

// --- Добавлено для рендеринга примитивов ---
// Шейдер для рисования цветных примитивов и текстур
static const char* vertex_shader_source =
    "attribute vec2 position;\n"
    "attribute vec2 texcoord;\n"
    "attribute vec4 color;\n"
    "varying vec2 frag_texcoord;\n"
    "varying vec4 frag_color;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "  gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
    "  frag_texcoord = texcoord;\n"
    "  frag_color = color;\n"
    "}\n";

static const char* fragment_shader_source_with_texture =
    "precision mediump float;\n"
    "varying vec2 frag_texcoord;\n"
    "varying vec4 frag_color;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "  vec4 tex_color = texture2D(tex, frag_texcoord);\n"
    "  gl_FragColor = frag_color * tex_color;\n"
    "}\n";

static const char* fragment_shader_source_no_texture =
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

// --- Добавлено для FreeType ---
int renderer_init_freetype(Renderer* renderer, const char* font_path) {
    if (!renderer || !font_path) {
        return 0;
    }

    // 1. Инициализируем FreeType библиотеку
    if (FT_Init_FreeType(&renderer->ft_library)) {
        log_error("Could not init FreeType library");
        return 0;
    }
    log_debug("FreeType library initialized");

    // 2. Загружаем шрифт
    if (FT_New_Face(renderer->ft_library, font_path, 0, &renderer->ft_face)) {
        log_error("Failed to load font from %s", font_path);
        FT_Done_FreeType(renderer->ft_library);
        return 0;
    }
    log_debug("Font loaded from %s", font_path);

    // 3. Устанавливаем размер шрифта
    FT_Set_Pixel_Sizes(renderer->ft_face, 0, 48); // 48 пикселей по высоте

    // 4. Создаем атлас текстур для глифов
    renderer->atlas_width = ATLAS_WIDTH;
    renderer->atlas_height = ATLAS_HEIGHT;
    renderer->texture_atlas_data = calloc(1, renderer->atlas_width * renderer->atlas_height);
    if (!renderer->texture_atlas_data) {
        log_error("Failed to allocate memory for texture atlas");
        FT_Done_Face(renderer->ft_face);
        FT_Done_FreeType(renderer->ft_library);
        return 0;
    }

    // 5. Генерируем атлас (упрощенно: только ASCII)
    int pen_x = 0;
    int pen_y = 0;
    int row_height = 0;
    for (unsigned char c = 32; c < 127; c++) { // ASCII 32-126
        if (FT_Load_Char(renderer->ft_face, c, FT_LOAD_RENDER)) {
            log_warn("Failed to load glyph for character %c", c);
            continue;
        }

        FT_GlyphSlot slot = renderer->ft_face->glyph;
        int bitmap_width = slot->bitmap.width;
        int bitmap_rows = slot->bitmap.rows;

        // Проверяем, помещается ли глиф в текущую строку атласа
        if (pen_x + bitmap_width >= renderer->atlas_width) {
            pen_x = 0;
            pen_y += row_height;
            row_height = 0;
        }

        // Проверяем, помещается ли строка в атлас
        if (pen_y + bitmap_rows >= renderer->atlas_height) {
            log_warn("Texture atlas is full, cannot add more glyphs");
            break;
        }

        // Копируем битмап глифа в атлас
        for (int row = 0; row < bitmap_rows; row++) {
            for (int col = 0; col < bitmap_width; col++) {
                int atlas_index = (pen_y + row) * renderer->atlas_width + (pen_x + col);
                int bitmap_index = row * slot->bitmap.width + col;
                renderer->texture_atlas_data[atlas_index] = slot->bitmap.buffer[bitmap_index];
            }
        }

        // Сохраняем UV координаты глифа (в реальной реализации это делается в отдельной структуре)
        // Пока просто логируем
        log_debug("Glyph '%c' placed at (%d, %d) size (%d, %d)", c, pen_x, pen_y, bitmap_width, bitmap_rows);

        pen_x += bitmap_width + 1; // 1 пиксель отступа
        if (bitmap_rows > row_height) {
            row_height = bitmap_rows;
        }
    }

    // 6. Создаем OpenGL текстуру из атласа
    glGenTextures(1, &renderer->texture_atlas_id);
    glBindTexture(GL_TEXTURE_2D, renderer->texture_atlas_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, renderer->atlas_width, renderer->atlas_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, renderer->texture_atlas_data);
    glBindTexture(GL_TEXTURE_2D, 0);

    renderer->is_freetype_initialized = 1;
    log_info("FreeType initialized successfully with font %s", font_path);
    return 1;
}

void renderer_cleanup_freetype(Renderer* renderer) {
    if (!renderer) return;

    if (renderer->texture_atlas_id) {
        glDeleteTextures(1, &renderer->texture_atlas_id);
        renderer->texture_atlas_id = 0;
    }
    if (renderer->texture_atlas_data) {
        free(renderer->texture_atlas_data);
        renderer->texture_atlas_data = NULL;
    }
    if (renderer->ft_face) {
        FT_Done_Face(renderer->ft_face);
        renderer->ft_face = NULL;
    }
    if (renderer->ft_library) {
        FT_Done_FreeType(renderer->ft_library);
        renderer->ft_library = NULL;
    }
    renderer->is_freetype_initialized = 0;
    log_info("FreeType resources cleaned up");
}
// --- Конец добавления для FreeType ---

Renderer* renderer_create(int width, int height) {
    Renderer* renderer = malloc(sizeof(Renderer));
    if (!renderer) {
        log_error("Failed to allocate memory for Renderer");
        return NULL;
    }
    memset(renderer, 0, sizeof(Renderer));
    renderer->width = width;
    renderer->height = height;
    // Инициализируем FreeType по умолчанию как неинициализированный
    renderer->is_freetype_initialized = 0;

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
    // Создаем шейдерную программу для текстурированных примитивов
    renderer->shader_program = create_program(vertex_shader_source, fragment_shader_source_with_texture);
    if (!renderer->shader_program) {
        log_error("Failed to create shader program for textured primitives");
        // Продолжаем, но рендеринг текста/примитивов не будет работать
    } else {
        renderer->pos_attrib = glGetAttribLocation(renderer->shader_program, "position");
        renderer->color_attrib = glGetAttribLocation(renderer->shader_program, "color");
        // Для текстурированного шейдера добавляем атрибут texcoord
        // GLint texcoord_attrib = glGetAttribLocation(renderer->shader_program, "texcoord");
        renderer->mvp_uniform = glGetUniformLocation(renderer->shader_program, "mvp");

        glGenBuffers(1, &renderer->vbo);
    }
    // --- Конец инициализации ресурсов ---

    // --- Попытка инициализировать FreeType ---
    // Пробуем сначала FreeType шрифт
    if (renderer_init_freetype(renderer, FREETYPE_FONT_PATH)) {
        log_info("FreeType initialized with primary font");
    } else {
        log_warn("Failed to initialize FreeType with primary font %s", FREETYPE_FONT_PATH);
        // Пробуем fallback шрифт
        if (renderer_init_freetype(renderer, TRUETYPE_FONT_PATH)) {
             log_info("FreeType initialized with fallback font");
        } else {
             log_warn("Failed to initialize FreeType with fallback font %s", TRUETYPE_FONT_PATH);
             log_warn("Text rendering will use placeholder rectangles");
        }
    }
    // --- Конец попытки инициализировать FreeType ---

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

    // --- Освобождение ресурсов FreeType ---
    renderer_cleanup_freetype(renderer);
    // --- Конец освобождения ресурсов FreeType ---

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
    if (!renderer || !text) {
        return;
    }

    // Если FreeType инициализирован, используем его
    if (renderer->is_freetype_initialized) {
        // TODO: Реализовать полноценный рендеринг через FreeType
        // Это сложная часть, требующая:
        // 1. Загрузки каждого глифа (FT_Load_Char)
        // 2. Обновления атласа текстур (если глиф новый)
        // 3. Расчета позиции и UV координат для каждого глифа
        // 4. Рисования серии текстурированных квадратов
        // Пока используем заглушку
        log_debug("FreeType is initialized, but full implementation is TODO. Using placeholder.");
        float text_width = strlen(text) * 8.0f * scale;
        float text_height = 16.0f * scale;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        renderer_draw_quad(x, y, text_width, text_height, r, g, b, a);
        return;
    }

    // Если FreeType не инициализирован, используем заглушку
    log_debug("FreeType not initialized, using placeholder for text: %.20s", text);
    float text_width = strlen(text) * 8.0f * scale;
    float text_height = 16.0f * scale;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    renderer_draw_quad(x, y, text_width, text_height, r, g, b, a);
}
// --- КОНЕЦ РЕАЛИЗАЦИИ ФУНКЦИИ РЕНДЕРИНГА ТЕКСТА ---
