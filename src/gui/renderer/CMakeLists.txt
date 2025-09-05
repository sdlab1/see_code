# src/gui/renderer/CMakeLists.txt

# - Подкаталог для модулей рендерера -
add_library(see_code_gl_components
    gl_context.c
    gl_shaders.c
    gl_primitives.c
    text_renderer.c
    freetype_text_renderer_init.c
    freetype_text_renderer_draw.c      # <<< Добавлено
    freetype_text_renderer_atlas.c     # <<< Добавлено
)

target_include_directories(see_code_gl_components
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}       # Для *.h файлов этого каталога
        ${CMAKE_CURRENT_SOURCE_DIR}/../   # Для renderer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../../ # Для core/config.h, utils/logger.h
)

# Линкуем зависимости для OpenGL/GLES, EGL, FreeType
target_link_libraries(see_code_gl_components
    GLESv2
    EGL
    freetype
    dl # Для dlopen в deps_check
)

# No need to install this library separately, it's part of see
