# src/gui/CMakeLists.txt

# Add the subdirectory for the text renderer module
add_subdirectory(renderer/text_renderer)

# GUI library
add_library(see_code_gui
    renderer.c
    ui_manager.c
    termux_gui_backend.c
    # Add other GUI source files as needed
)

# Specify include directories for the main GUI library
target_include_directories(see_code_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} # For renderer.h, ui_manager.h etc.
    ${CMAKE_CURRENT_SOURCE_DIR}/../ # For core/config.h etc.
)

# Link the main GUI library against its dependencies and the new text renderer
target_link_libraries(see_code_gui
    GLESv2
    EGL
    see_code_text_renderer # Link the new text rendering module
    # Add other necessary libraries like pthread, dl, freetype if needed directly here or via text_renderer
)
