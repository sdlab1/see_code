# CMakeLists.txt (в корне проекта)
cmake_minimum_required(VERSION 3.10)
project(see_code C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Определяем пути к исходникам ---
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- Добавляем директории для поиска заголовочных файлов ---
# ВАЖНО: Указываем путь к src/, так как заголовки там (#include "see_code/core/app.h")
include_directories(${SRC_DIR})

# --- Все библиотеки и исполняемый файл определяются прямо здесь ---

# Core library
add_library(see_code_core
    ${SRC_DIR}/core/app.c
)

# GUI library (включает все GUI-модули)
add_library(see_code_gui
    # --- Модули рендерера ---
    ${SRC_DIR}/gui/renderer/gl_context.c
    ${SRC_DIR}/gui/renderer/gl_shaders.c
    ${SRC_DIR}/gui/renderer/gl_primitives.c
    ${SRC_DIR}/gui/renderer/text_renderer.c
    ${SRC_DIR}/gui/renderer/text_renderer_core.c
    ${SRC_DIR}/gui/renderer/text_renderer_draw.c
    # --- Основной рендерер ---
    ${SRC_DIR}/gui/renderer.c
    # --- UI Manager (все модули) ---
    ${SRC_DIR}/gui/ui_manager.c
    ${SRC_DIR}/gui/ui_manager_core.c
    ${SRC_DIR}/gui/ui_manager_input.c
    ${SRC_DIR}/gui/ui_manager_render.c
    # --- Termux GUI Backend ---
    ${SRC_DIR}/gui/termux_gui_backend.c
)

# Network library
add_library(see_code_network
    ${SRC_DIR}/network/socket_server.c
)

# Data library
add_library(see_code_data
    ${SRC_DIR}/data/diff_data.c
    ${SRC_DIR}/data/diff_parser.c
)

# Utils library
add_library(see_code_utils
    ${SRC_DIR}/utils/logger.c
    ${SRC_DIR}/utils/deps_check.c
)

# --- Линкуем необходимые библиотеки ---
# Core library
target_link_libraries(see_code_core
    see_code_gui
    see_code_network
    see_code_data
    see_code_utils
    GLESv2
    EGL
    pthread
    dl # Для dlopen в deps_check
)

# GUI library
target_link_libraries(see_code_gui
    GLESv2
    EGL
    freetype
    pthread
    dl # Для dlopen в deps_check
)

# Network library
target_link_libraries(see_code_network
    pthread
)

# Data library
# Нет внешних зависимостей, кроме стандартной библиотеки C

# Utils library
target_link_libraries(see_code_utils
    dl # Для dlopen в deps_check
)

# --- Создаем исполняемый файл ---
add_executable(see_code
    ${SRC_DIR}/core/main.c
)

# --- Линкуем необходимые библиотеки для исполняемого файла ---
target_link_libraries(see_code
    see_code_core
    see_code_gui
    see_code_network
    see_code_data
    see_code_utils
    GLESv2
    EGL
    pthread
    dl # Для dlopen в deps_check
    freetype
)

# --- Устанавливаем исполняемый файл в префикс/bin ---
install(TARGETS see_code DESTINATION bin)

# --- Устанавливаем плагин Neovim ---
install(
    FILES plugin/see_code.lua
    DESTINATION share/nvim/site/plugin
    RENAME see_code.lua
)
