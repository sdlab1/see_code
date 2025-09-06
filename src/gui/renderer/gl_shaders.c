// src/gui/renderer/gl_shaders.c
#include "see_code/gui/renderer/gl_shaders.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>

// --- ОБНОВЛЕННЫЕ ШЕЙДЕРЫ ДЛЯ БАТЧИНГА ---

// Вершинный шейдер для текстур (текста) и сплошных цветов
const char* gl_shaders_batch_vertex_shader_source =
    "#version 100\n"
    "attribute vec2 a_position;\n"   // Позиция
    "attribute vec2 a_texcoord;\n"   // UV-координаты
    "attribute vec4 a_color;\n"      // Цвет вершины
    "varying vec2 v_texcoord;\n"
    "varying vec4 v_color;\n"
    "uniform mat4 u_mvp;\n"
    "void main() {\n"
    "  gl_Position = u_mvp * vec4(a_position, 0.0, 1.0);\n"
    "  v_texcoord = a_texcoord;\n"
    "  v_color = a_color;\n"
    "}\n";

// Фрагментный шейдер для текстур (текста)
const char* gl_shaders_batch_textured_fragment_source =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "varying vec4 v_color;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "  // Альфа-канал берется из текстуры (для глифа), цвет - из атрибута\n"
    "  gl_FragColor = v_color * texture2D(u_texture, v_texcoord);\n"
    "}\n";

// Фрагментный шейдер для сплошного цвета
const char* gl_shaders_batch_solid_fragment_source =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_FragColor = v_color;\n"
    "}\n";

// --- КОНЕЦ ОБНОВЛЕННЫХ ШЕЙДЕРОВ ---

GLuint gl_shaders_compile_shader(GLenum type, const char* source) {
    if (!source) {
        log_error("Shader source is NULL");
        return 0;
    }
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
            log_error("Shader compilation failed (%s): %s",
                      (type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment", info_log);
            free(info_log);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint gl_shaders_create_program(GLuint vertex_shader, GLuint fragment_shader) {
    if (!vertex_shader || !fragment_shader) {
        log_error("Invalid shader IDs provided");
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
        return 0;
    }
    return program;
}

GLuint gl_shaders_create_program_from_sources(const char* vertex_source, const char* fragment_source) {
    GLuint vertex_shader = gl_shaders_compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (!vertex_shader) return 0;
    GLuint fragment_shader = gl_shaders_compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!fragment_shader) {
        glDeleteShader(vertex_shader);
        return 0;
    }
    GLuint program = gl_shaders_create_program(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}
