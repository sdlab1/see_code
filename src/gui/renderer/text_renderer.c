// src/gui/renderer.c
#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>

// --- Forward declarations for new modules ---
// In a full refactoring, these would come from their respective headers
// gl_context.h
typedef struct GLContext GLContext;
GLContext* gl_context_create(int width, int height);
void gl_context_destroy(GLContext* ctx);
int gl_context_begin_frame(GLContext* ctx);
int gl_context_end_frame(GLContext* ctx);
void gl_context_resize(GLContext* ctx, int width, int height);
void gl_context_clear(GLContext* ctx, float r, float g, float b, float a);
// gl_shaders.h
GLuint gl_shaders_compile_shader(GLenum type, const char* source);
GLuint gl_shaders_create_program(GLuint vertex_shader, GLuint fragment_shader);
GLuint gl_shaders_create_program_from_sources(const char* vertex_source, const char* fragment_source);
extern const char* gl_shaders_textured_vertex_shader_source;
extern const char* gl_shaders_textured_fragment_shader_source;
extern const char* gl_shaders_solid_vertex_shader_source;
extern const char* gl_shaders_solid_fragment_shader_source;
// gl_primitives.h
int gl_primitives_draw_solid_quad(GLuint program_id, float x, float y, float width, float height,
                                 float r, float g, float b, float a,
                                 const float mvp[16], GLuint vbo_id);
int gl_primitives_draw_textured_quad(GLuint program_id, GLuint texture_id,
                                    float x, float y, float width, float height,
                                    float u0, float v0, float u1, float v1,
                                    unsigned int color_rgba,
                                    const float mvp[16], GLuint vbo_id);
// text_renderer.h (уже существует)
// Предполагаем, что text_renderer_init, text_renderer_cleanup, text_renderer_draw_text определены
// --- End Forward declarations ---

// --- Internal data structures for new modules ---
// In a full refactoring, these would be opaque in the header
struct GLContext {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int width;
    int height;
};
// gl_shaders, gl_primitives don't expose internal state in this design
// --- End Internal data structures ---

// --- Updated Renderer structure ---
// Now holds handles to the modular components
struct Renderer {
    GLContext* gl_ctx;
    // Shaders
    GLuint shader_program_solid;
    GLuint shader_program_textured;
    // Primitives/VBOs
    GLuint vbo_solid;
    GLuint vbo_textured;
    // Text rendering data (moved here for simplicity in this step)
    // In a full refactor, text_renderer would manage its own data
    struct TextRendererInternalData* text_internal_data; // For interaction with text_renderer
    int width;
    int height;
};
// --- End Updated Renderer structure ---

// --- Helper functions for accessing internal text data ---
// These would ideally be declared in renderer.h or managed differently
struct TextRendererInternalData {
    int is_placeholder; // Just to make it a valid struct
    // Actual fields would be defined in text_renderer.c
};
struct TextRendererInternalData* renderer_get_internal_text_data(const Renderer* renderer) {
    return renderer ? renderer->text_internal_data : NULL;
}
void renderer_set_internal_text_data(Renderer* renderer, struct TextRendererInternalData* data) {
    if (renderer) renderer->text_internal_data = data;
}
// --- End Helper functions ---

Renderer* renderer_create(int width, int height) {
    Renderer* renderer = malloc(sizeof(Renderer));
    if (!renderer) {
        log_error("Failed to allocate memory for Renderer");
        return NULL;
    }
    memset(renderer, 0, sizeof(Renderer));
    renderer->width = width;
    renderer->height = height;

    log_info("Initializing modular renderer components...");

    // --- 1. Initialize GL Context ---
    renderer->gl_ctx = gl_context_create(width, height);
    if (!renderer->gl_ctx) {
        log_error("Failed to initialize GL context");
        free(renderer);
        return NULL;
    }
    log_debug("GL Context initialized");

    // --- 2. Initialize Shaders ---
    // Solid color shader
    renderer->shader_program_solid = gl_shaders_create_program_from_sources(
        gl_shaders_solid_vertex_shader_source,
        gl_shaders_solid_fragment_shader_source
    );
    if (!renderer->shader_program_solid) {
        log_error("Failed to create solid shader program");
        // Not critical, but rendering will be broken
    } else {
        log_debug("Solid shader program created");
    }

    // Textured shader (for text)
    renderer->shader_program_textured = gl_shaders_create_program_from_sources(
        gl_shaders_textured_vertex_shader_source,
        gl_shaders_textured_fragment_shader_source
    );
    if (!renderer->shader_program_textured) {
        log_error("Failed to create textured shader program");
        // Not critical for basic quads, but text rendering will be broken
    } else {
        log_debug("Textured shader program created");
    }

    // --- 3. Initialize Primitive Resources (VBOs) ---
    // glGenBuffers is a GLES2 function, so we need a context current
    // gl_context_begin_frame makes context current
    if (!gl_context_begin_frame(renderer->gl_ctx)) {
        log_error("Failed to make context current for VBO creation");
        gl_context_destroy(renderer->gl_ctx);
        free(renderer);
        return NULL;
    }

    glGenBuffers(1, &renderer->vbo_solid);
    glGenBuffers(1, &renderer->vbo_textured);
    log_debug("Primitive VBOs created");

    // We are done with immediate GL calls for setup
    // gl_context_end_frame would swap buffers, which is not needed here
    // Context will be made current again in renderer_begin_frame

    log_info("Modular renderer components initialized");
    return renderer;
}

void renderer_destroy(Renderer* renderer) {
    if (!renderer) {
        return;
    }

    log_info("Cleaning up modular renderer components...");

    // --- Cleanup in reverse order ---
    if (renderer->vbo_solid) glDeleteBuffers(1, &renderer->vbo_solid);
    if (renderer->vbo_textured) glDeleteBuffers(1, &renderer->vbo_textured);
    log_debug("Primitive VBOs deleted");

    if (renderer->shader_program_solid) glDeleteProgram(renderer->shader_program_solid);
    if (renderer->shader_program_textured) glDeleteProgram(renderer->shader_program_textured);
    log_debug("Shader programs deleted");

    if (renderer->gl_ctx) {
        gl_context_destroy(renderer->gl_ctx);
        log_debug("GL Context destroyed");
    }

    // Note: text_internal_data cleanup is handled by text_renderer_cleanup
    // which is called from app.c/renderer manager

    free(renderer);
    log_info("Modular renderer components cleaned up");
}

int renderer_begin_frame(Renderer* renderer) {
    if (!renderer || !renderer->gl_ctx) {
        return 0;
    }
    // Delegate to gl_context
    return gl_context_begin_frame(renderer->gl_ctx);
    // MVP matrix setup, clearing, etc., would happen here or be managed by caller/UIManager
}

int renderer_end_frame(Renderer* renderer) {
    if (!renderer || !renderer->gl_ctx) {
        return 0;
    }
    // Delegate to gl_context
    return gl_context_end_frame(renderer->gl_ctx);
    // Swapping buffers happens inside gl_context_end_frame
}

void renderer_resize(Renderer* renderer, int width, int height) {
    if (!renderer || !renderer->gl_ctx) {
        return;
    }
    renderer->width = width;
    renderer->height = height;
    // Delegate to gl_context
    gl_context_resize(renderer->gl_ctx, width, height);
    // Viewport update happens inside gl_context_resize
}

void renderer_clear(float r, float g, float b, float a) {
    if (!renderer || !renderer->gl_ctx) {
        // Fallback clear or log?
        log_debug("Renderer or GL context is NULL in renderer_clear");
        return;
    }
    // Delegate to gl_context
    gl_context_clear(renderer->gl_ctx, r, g, b, a);
    // glClearColor and glClear happen inside gl_context_clear
}

void renderer_draw_quad(float x, float y, float width, float height,
                       float r, float g, float b, float a) {
    if (!renderer || !renderer->gl_ctx) {
        log_debug("Renderer or GL context is NULL in renderer_draw_quad");
        return; // Or fallback rendering?
    }

    // Create a simple orthographic MVP matrix
    // This should ideally be passed in or managed by a higher-level scene/graphic manager
    float mvp[16] = {
        2.0f / renderer->width, 0, 0, 0,
        0, -2.0f / renderer->height, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1
    };

    // Delegate to gl_primitives
    if (!gl_primitives_draw_solid_quad(renderer->shader_program_solid, x, y, width, height,
                                      r, g, b, a, mvp, renderer->vbo_solid)) {
        log_warn("Failed to draw solid quad using gl_primitives");
        // Fallback?
    }
}

// --- Delegated to text_renderer module ---
void renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text) {
        return;
    }
    // Delegate entirely to the text_renderer module
    // The text_renderer module knows how to access renderer's GL context/shaders if needed
    // via renderer_get_internal_text_data or similar mechanisms
    extern void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color); // Forward decl
    text_renderer_draw_text(renderer, text, x, y, scale, color);
}
// --- End delegation to text_renderer ---

// Getters for width/height (useful for MVP calculation, UI layout etc.)
int renderer_get_width(const Renderer* renderer) {
    return renderer ? renderer->width : 0;
}

int renderer_get_height(const Renderer* renderer) {
    return renderer ? renderer->height : 0;
}
