# src/gui/renderer/text_renderer/CMakeLists.txt

# Add the text renderer library
add_library(see_code_text_renderer
    text_renderer.c
)

# Specify include directories for this library
# It needs to see headers from its own directory and the parent gui directory
target_include_directories(see_code_text_renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} # For text_renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ # For renderer.h, config.h etc.
)

# Link against required libraries
# GLESv2, EGL are needed for OpenGL calls (if text_renderer_draw_text evolves)
# freetype is needed for FT_ calls
target_link_libraries(see_code_text_renderer
    GLESv2
    EGL
    freetype
)

# No need to install this library separately, it's part of see_code_gui
