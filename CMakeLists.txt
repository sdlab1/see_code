# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(see_code C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${SRC_DIR})

# --- Библиотеки проекта ---
add_library(see_code_core ${SRC_DIR}/core/app.c)

add_library(see_code_gui
    ${SRC_DIR}/gui/renderer/gl_context.c
    ${SRC_DIR}/gui/renderer/gl_shaders.c
    # gl_primitives.c УДАЛЕН
    ${SRC_DIR}/gui/renderer/text_renderer.c
    ${SRC_DIR}/gui/renderer.c
    ${SRC_DIR}/gui/ui_manager_widgets.c
    ${SRC_DIR}/gui/ui_manager_core.c
    ${SRC_DIR}/gui/ui_manager_input.c
    ${SRC_DIR}/gui/ui_manager_render.c
    ${SRC_DIR}/gui/termux_gui_backend.c
)

add_library(see_code_network ${SRC_DIR}/network/socket_server.c)
add_library(see_code_data ${SRC_DIR}/data/diff_data.c ${SRC_DIR}/data/diff_parser.c)
add_library(see_code_utils ${SRC_DIR}/utils/logger.c ${SRC_DIR}/utils/deps_check.c)

# --- Линковка ---
target_link_libraries(see_code_core PUBLIC see_code_gui see_code_network see_code_data see_code_utils pthread)
target_link_libraries(see_code_gui PUBLIC GLESv2 EGL freetype dl)
target_link_libraries(see_code_utils PUBLIC dl)

# --- Исполняемый файл ---
add_executable(see_code ${SRC_DIR}/core/main.c)
target_link_libraries(see_code see_code_core)

# --- Установка ---
install(TARGETS see_code DESTINATION bin)
install(FILES plugin/see_code.lua DESTINATION share/nvim/site/plugin RENAME see_code.lua)
