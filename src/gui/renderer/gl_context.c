// src/gui/renderer/gl_context.c
#include "see_code/gui/renderer/gl_context.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>

struct GLContext {
    EGLDisplay display;
    EGLSurface surface; // Placeholder
    EGLContext context;
    int width;
    int height;
};

GLContext* gl_context_create(int width, int height) {
    GLContext* ctx = malloc(sizeof(GLContext));
    if (!ctx) {
        log_error("Failed to allocate memory for GLContext");
        return NULL;
    }
    memset(ctx, 0, sizeof(GLContext));
    ctx->width = width;
    ctx->height = height;
    ctx->display = EGL_NO_DISPLAY;
    ctx->surface = EGL_NO_SURFACE;
    ctx->context = EGL_NO_CONTEXT;

    ctx->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (ctx->display == EGL_NO_DISPLAY) {
        log_error("Failed to get EGL display");
        free(ctx);
        return NULL;
    }

    if (!eglInitialize(ctx->display, NULL, NULL)) {
        log_error("Failed to initialize EGL");
        free(ctx);
        return NULL;
    }

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
    if (!eglChooseConfig(ctx->display, config_attribs, &config, 1, &num_configs) || num_configs == 0) {
        log_error("Failed to choose EGL config");
        eglTerminate(ctx->display);
        free(ctx);
        return NULL;
    }

    // Create a placeholder surface. In a real app, Termux:GUI would provide this.
    ctx->surface = eglCreateWindowSurface(ctx->display, config, NULL, NULL);
    if (ctx->surface == EGL_NO_SURFACE) {
        log_error("Failed to create EGL surface (placeholder)");
        eglTerminate(ctx->display);
        free(ctx);
        return NULL;
    }

    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    ctx->context = eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, context_attribs);
    if (ctx->context == EGL_NO_CONTEXT) {
        log_error("Failed to create EGL context");
        eglDestroySurface(ctx->display, ctx->surface);
        eglTerminate(ctx->display);
        free(ctx);
        return NULL;
    }

    if (!eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context)) {
        log_error("Failed to make EGL context current");
        eglDestroyContext(ctx->display, ctx->context);
        eglDestroySurface(ctx->display, ctx->surface);
        eglTerminate(ctx->display);
        free(ctx);
        return NULL;
    }

    log_info("GLContext initialized successfully");
    return ctx;
}

void gl_context_destroy(GLContext* ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (ctx->context != EGL_NO_CONTEXT) {
            eglDestroyContext(ctx->display, ctx->context);
        }
        if (ctx->surface != EGL_NO_SURFACE) {
            eglDestroySurface(ctx->display, ctx->surface);
        }
        eglTerminate(ctx->display);
    }
    free(ctx);
}

int gl_context_begin_frame(GLContext* ctx) {
    if (!ctx) {
        return 0;
    }
    glViewport(0, 0, ctx->width, ctx->height);
    return 1;
}

int gl_context_end_frame(GLContext* ctx) {
    if (!ctx) {
        return 0;
    }
    if (!eglSwapBuffers(ctx->display, ctx->surface)) {
        log_error("Failed to swap EGL buffers");
        return 0;
    }
    return 1;
}

void gl_context_resize(GLContext* ctx, int width, int height) {
    if (!ctx) {
        return;
    }
    ctx->width = width;
    ctx->height = height;
    glViewport(0, 0, width, height);
}

void gl_context_clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

// --- Геттеры ---
EGLDisplay gl_context_get_display(const GLContext* ctx) {
    return ctx ? ctx->display : EGL_NO_DISPLAY;
}
EGLSurface gl_context_get_surface(const GLContext* ctx) {
    return ctx ? ctx->surface : EGL_NO_SURFACE;
}
EGLContext gl_context_get_context(const GLContext* ctx) {
    return ctx ? ctx->context : EGL_NO_CONTEXT;
}
int gl_context_get_width(const GLContext* ctx) {
    return ctx ? ctx->width : 0;
}
int gl_context_get_height(const GLContext* ctx) {
    return ctx ? ctx->height : 0;
}
// --- Конец геттеров ---
