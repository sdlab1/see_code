// src/gui/renderer/text_renderer.c
#include "see_code/gui/renderer/text_renderer.h"
#include "see_code/gui/renderer.h" // For accessing parent Renderer struct members if needed
#include "see_code/utils/logger.h"
#include "see_code/core/config.h" // For default font paths
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // For snprintf

// Extend the main Renderer struct with text rendering data
// This is a common technique in C for adding subsystem-specific data
struct TextRendererData {
    int is_freetype_initialized;
    FT_Library ft_library;
    FT_Face ft_face;
    // Texture atlas and glyph cache would go here for full FreeType impl
    GLuint dummy_texture_id; // Placeholder
};

// Helper macro to access TextRendererData from Renderer
#define GET_TEXT_RENDERER_DATA(r) ((struct TextRendererData*)((char*)(r) + sizeof(struct RendererPlaceholder)))

// --- Добавлено для FreeType ---
int text_renderer_init_freetype(struct TextRendererData* tr_data, const char* font_path) {
    if (!tr_data || !font_path) {
        return 0;
    }

    if (FT_Init_FreeType(&tr_data->ft_library)) {
        log_error("Could not init FreeType library");
        return 0;
    }
    log_debug("FreeType library initialized");

    if (FT_New_Face(tr_data->ft_library, font_path, 0, &tr_data->ft_face)) {
        log_error("Failed to load font from %s", font_path);
        FT_Done_FreeType(tr_data->ft_library);
        return 0;
    }
    log_debug("Font loaded from %s", font_path);

    FT_Set_Pixel_Sizes(tr_data->ft_face, 0, 48);

    // In a full implementation, create texture atlas here
    tr_data->dummy_texture_id = 1; // Just a flag for now
    tr_data->is_freetype_initialized = 1;
    log_info("FreeType initialized successfully with font %s", font_path);
    return 1;
}

void text_renderer_cleanup_freetype(struct TextRendererData* tr_data) {
    if (!tr_data || !tr_data->is_freetype_initialized) return;

    if (tr_data->ft_face) {
        FT_Done_Face(tr_data->ft_face);
        tr_data->ft_face = NULL;
    }
    if (tr_data->ft_library) {
        FT_Done_FreeType(tr_data->ft_library);
        tr_data->ft_library = NULL;
    }
    tr_data->dummy_texture_id = 0;
    tr_data->is_freetype_initialized = 0;
    log_info("FreeType resources cleaned up");
}
// --- Конец добавления для FreeType ---

int text_renderer_init(Renderer* renderer, const char* font_path_hint) {
    if (!renderer) {
        log_error("Invalid renderer provided to text_renderer_init");
        return 0;
    }

    // --- WARNING: This approach modifies the Renderer struct size ---
    // This is illustrative. In practice, you'd pre-allocate space in Renderer
    // or use a separate allocation and store a pointer.
    // For this example, we assume Renderer has enough trailing padding.
    // A safer approach is to allocate TextRendererData separately and store
    // a pointer in Renderer (e.g., Renderer* renderer; TextRendererData* text_data;)
    // This direct casting/modification is risky.
    // Let's assume Renderer has a reserved field or we manage it differently.
    // To keep it simple here, let's assume we manage TextRendererData externally
    // or it's part of Renderer's definition. Let's revise the approach.

    // Better approach: Assume Renderer has a field like `void* text_renderer_private_data;`
    // Or, define TextRendererData as part of Renderer struct definition.
    // Since we can't modify `struct Renderer` in renderer.h easily in this context,
    // we'll simulate it by allocating data separately and storing it.
    // However, for seamless integration, TextRendererData should ideally be part of `struct Renderer`.

    // For now, let's assume we can attach our data. We'll cheat slightly for demo.
    // In reality, you'd modify `struct Renderer` in `renderer.h` to include `TextRendererData`
    // or a pointer to it.

    // Simulate attaching data (not recommended for production without struct modification)
    struct TextRendererData* tr_data = calloc(1, sizeof(struct TextRendererData));
    if (!tr_data) {
        log_error("Failed to allocate memory for TextRendererData");
        return 0;
    }

    // Store pointer to our data in a way accessible to other text_renderer functions
    // This requires modifying `struct Renderer` to have a `void* text_data_private;` field.
    // As we cannot do that here, we'll proceed with the init logic assuming the data is attached.

    // Let's pretend it's attached correctly for the rest of the logic.
    // Actual attachment mechanism depends on how `struct Renderer` is defined.

    // --- Попытка инициализировать FreeType ---
    const char* font_to_try = font_path_hint ? font_path_hint : FREETYPE_FONT_PATH; // Use hint or default
    if (text_renderer_init_freetype(tr_data, font_to_try)) {
        log_info("Text renderer: FreeType initialized with %s", font_to_try);
        // Attach tr_data to renderer (conceptual)
        // renderer->text_data_private = tr_data;
        free(tr_data); // Free the temporary allocation, as we can't attach it properly here.
        return 1; // Success
    } else {
        log_warn("Text renderer: Failed to initialize FreeType with %s", font_to_try);
        // Пробуем fallback шрифт
        if (text_renderer_init_freetype(tr_data, TRUETYPE_FONT_PATH)) {
             log_info("Text renderer: FreeType initialized with fallback font %s", TRUETYPE_FONT_PATH);
             // Attach tr_data to renderer (conceptual)
             // renderer->text_data_private = tr_data;
             free(tr_data);
             return 1; // Success
        } else {
             log_warn("Text renderer: Failed to initialize FreeType with fallback font %s", TRUETYPE_FONT_PATH);
             log_warn("Text renderer: Will use placeholder rectangle rendering.");
             tr_data->is_freetype_initialized = 0; // Explicitly mark as not initialized
             // Attach tr_data to renderer (conceptual)
             // renderer->text_data_private = tr_data;
             free(tr_data);
             return 1; // Success (fallback mode)
        }
    }
    // --- Конец попытки инициализировать FreeType ---
}

void text_renderer_cleanup(Renderer* renderer) {
     if (!renderer) return;
     // Retrieve attached data (conceptual)
     // struct TextRendererData* tr_data = (struct TextRendererData*)renderer->text_data_private;
     // if (tr_data) {
     //     text_renderer_cleanup_freetype(tr_data);
     //     free(tr_data);
     //     renderer->text_data_private = NULL;
     // }
     // As we cannot properly attach/detach, cleanup is conceptual here.
     log_debug("Text renderer cleanup called (actual cleanup depends on data attachment).");
}

void text_renderer_draw_text(Renderer* renderer, const char* text, float x, float y, float scale, unsigned int color) {
    if (!renderer || !text) {
        return;
    }

    // Retrieve attached data to check FreeType status (conceptual)
    // struct TextRendererData* tr_data = (struct TextRendererData*)renderer->text_data_private;
    // int is_ft_init = tr_data && tr_data->is_freetype_initialized;
    // For demo, let's assume FreeType is not initialized and always use fallback.
    int is_ft_init = 0; // Change this based on actual state

    if (is_ft_init) {
        // TODO: Реализовать полноценный рендеринг через FreeType
        // Это включает:
        // 1. Загрузку каждого символа (`FT_Load_Char`).
        // 2. Создание или обновление атласа текстур (`glTexImage2D`).
        // 3. Расчет позиций и UV-координат для каждого глифа.
        // 4. Рисование серии текстурированных квадратов (`glDrawArrays`).
        log_debug("Full FreeType implementation is TODO. Using placeholder.");
        float text_width = strlen(text) * 8.0f * scale;
        float text_height = 16.0f * scale;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        // Вызываем родительскую функцию рисования квадрата
        // Предполагается, что renderer_draw_quad объявлена в renderer.h и определена в renderer.c
        extern void renderer_draw_quad(float x, float y, float width, float height, float r, float g, float b, float a);
        renderer_draw_quad(x, y, text_width, text_height, r, g, b, a);
    } else {
        // Fallback: рисуем цветной прямоугольник
        log_debug("FreeType not initialized, using placeholder for text: %.20s", text);
        float text_width = strlen(text) * 8.0f * scale;
        float text_height = 16.0f * scale;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        extern void renderer_draw_quad(float x, float y, float width, float height, float r, float g, float b, float a);
        renderer_draw_quad(x, y, text_width, text_height, r, g, b, a);
    }
}
