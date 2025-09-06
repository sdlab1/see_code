// src/gui/renderer/gl_shaders.c
#include "see_code/gui/renderer/gl_shaders.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>

// --- Определения предопределенных шейдеров ---
const char* gl_shaders_textured_vertex_shader_source =
    "#version 100\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 frag_texcoord;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "  gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
    "  frag_texcoord = texcoord;\n"
    "}\n";

const char* gl_shaders_textured_fragment_shader_source =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 frag_texcoord;\n"
    "uniform sampler2D tex;\n"
    "uniform vec4 text_color;\n"
    "void main() {\n"
    "  vec4 tex_color = texture2D(tex, frag_texcoord);\n"
    "  gl_FragColor = text_color * tex_color;\n"
    "}\n";

const char* gl_shaders_solid_vertex_shader_source =
    "#version 100\n"
    "attribute vec2 position;\n"
    "attribute vec4 color;\n"
    "varying vec4 frag_color;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "  gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
    "  frag_color = color;\n"
    "}\n";

const char* gl_shaders_solid_fragment_shader_source =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec4 frag_color;\n"
    "void main() {\n"
    "  gl_FragColor = frag_color;\n"
    "}\n";
// --- Конец определений ---

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
    log_debug("Shader compiled successfully (%s)",
              (type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment");
    return shader;
}

GLuint gl_shaders_create_program(GLuint vertex_shader, GLuint fragment_shader) {
    if (!vertex_shader || !fragment_shader) {
        log_error("Invalid shader IDs provided to gl_shaders_create_program");
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
    log_debug("Shader program linked successfully");
    return program;
}

GLuint gl_shaders_create_program_from_sources(const char* vertex_source, const char* fragment_source) {
    if (!vertex_source || !fragment_source) {
        log_error("Shader sources are NULL");
        return 0;
    }

    GLuint vertex_shader = gl_shaders_compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (!vertex_shader) {
        log_error("Failed to compile vertex shader");
        return 0;
    }

    GLuint fragment_shader = gl_shaders_compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!fragment_shader) {
        log_error("Failed to compile fragment shader");
        glDeleteShader(vertex_shader);
        return 0;
    }

    GLuint program = gl_shaders_create_program(vertex_shader, fragment_shader);

    // Шейдеры можно от detach-ить и удалить после линковки, если они больше не нужны напрямую
    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    if (!program) {
        log_error("Failed to create program from sources");
        return 0;
    }

    return program;
}

// Global shader program and VBO for solid rendering
static GLuint g_solid_shader_program = 0;
static GLuint g_solid_vbo = 0;
static int g_gl_shaders_initialized = 0;

// Initialize the global solid shader program
static int gl_shaders_init_solid(void) {
    if (g_gl_shaders_initialized) return 1;
    
    g_solid_shader_program = gl_shaders_create_program_from_sources(
        gl_shaders_solid_vertex_shader_source,
        gl_shaders_solid_fragment_shader_source
    );
    
    if (!g_solid_shader_program) {
        log_error("Failed to create solid shader program");
        return 0;
    }
    
    // Create VBO
    glGenBuffers(1, &g_solid_vbo);
    if (g_solid_vbo == 0) {
        log_error("Failed to create solid VBO");
        glDeleteProgram(g_solid_shader_program);
        g_solid_shader_program = 0;
        return 0;
    }
    
    g_gl_shaders_initialized = 1;
    log_debug("Solid shader system initialized");
    return 1;
}

// Get the solid shader program (initialize if needed)
GLuint gl_shaders_get_solid_program(void) {
    if (!g_gl_shaders_initialized) {
        gl_shaders_init_solid();
    }
    return g_solid_shader_program;
}

// Get the solid VBO (initialize if needed)  
GLuint gl_primitives_get_solid_vbo(void) {
    if (!g_gl_shaders_initialized) {
        gl_shaders_init_solid();
    }
    return g_solid_vbo;
}

// Cleanup function (call from renderer cleanup)
void gl_shaders_cleanup(void) {
    if (g_solid_shader_program) {
        glDeleteProgram(g_solid_shader_program);
        g_solid_shader_program = 0;
    }
    if (g_solid_vbo) {
        glDeleteBuffers(1, &g_solid_vbo);
        g_solid_vbo = 0;
    }
    g_gl_shaders_initialized = 0;
}
