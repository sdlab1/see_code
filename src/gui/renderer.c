#include "see_code/gui/renderer.h"
#include "see_code/utils/logger.h"

#include <stdlib.h>
#include <string.h>

struct Renderer {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int width;
    int height;
};

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
    // For now, we'll create a placeholder
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
    
    log_info("Renderer initialized with GLES2 context");
    return renderer;
}

void renderer_destroy(Renderer* renderer) {
    if (!renderer) {
        return;
    }
    
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
    // In a real implementation, you would use shaders and VBOs
    // This is just a placeholder
    
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
