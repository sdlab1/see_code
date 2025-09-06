// src/gui/renderer.h - Fixed function signatures
#ifndef SEE_CODE_RENDERER_H
#define SEE_CODE_RENDERER_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>

// Forward declarations
typedef struct Renderer Renderer;

// Renderer functions
Renderer* renderer_create(int width, int height);
void renderer_destroy(Renderer* renderer);
int renderer_begin_frame(Renderer* renderer);
int renderer_end_frame(Renderer* renderer);
void renderer_resize(Renderer* renderer, int width, int height);

// Rendering functions - FIXED: Add Renderer parameter
void renderer_clear(Renderer* renderer, float r, float g, float b, float a);
void renderer_draw_quad(Renderer* renderer, float x, float y, float width, float height,
                       float r, float g, float b, float a);

// Text rendering
void renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color);

// Getters
int renderer_get_width(const Renderer* renderer);
int renderer_get_height(const Renderer* renderer);

#endif // SEE_CODE_RENDERER_H
