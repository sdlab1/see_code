# src/CMakeLists.txt
# Core library
add_library(see_code_core
    core/app.c
    # config.h is a header file, no .c file to compile
)
target_include_directories(see_code_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include # If you have an include dir
    ${CMAKE_CURRENT_SOURCE_DIR} # For relative includes like see_code/core/...
)
# No specific libraries needed for core itself, it uses others

# GUI library
add_library(see_code_gui
    gui/renderer.c
    gui/ui_manager.c
    gui/termux_gui_backend.c
)
target_include_directories(see_code_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(see_code_gui
    GLESv2
    EGL
    # freetype if used directly, otherwise rely on app check
)

# Network library
add_library(see_code_network
    network/socket_server.c
)
target_include_directories(see_code_network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# No specific external libs for network

# Data library
add_library(see_code_data
    data/diff_data.c
)
target_include_directories(see_code_data PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# --- CHANGED: Removed cjson from target_link_libraries ---
# target_link_libraries(see_code_data cjson) REMOVED
# --- END CHANGE ---

# Utils library
add_library(see_code_utils
    utils/logger.c
    utils/deps_check.c # This file needs updating to remove cjson check
)
target_include_directories(see_code_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(see_code_utils
    dl # For dlopen in deps_check
)
