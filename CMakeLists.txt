# CMakeLists.txt (в корне проекта)
cmake_minimum_required(VERSION 3.10)
project(see_code C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

include_directories(
    ${SRC_DIR}
    ${SRC_DIR}/core
    ${SRC_DIR}/data
    ${SRC_DIR}/gui
    ${SRC_DIR}/network
    ${SRC_DIR}/utils
)

add_subdirectory(src)

add_executable(see_code
    src/core/main.c
)

# --- CHANGED: Removed cjson from target_link_libraries ---
target_link_libraries(see_code
    see_code_core
    see_code_gui
    see_code_network
    see_code_data
    see_code_utils
    GLESv2
    EGL
    # cjson REMOVED
    pthread
    dl # For dlopen in deps_check
)
# --- END CHANGE ---

install(TARGETS see_code DESTINATION bin)
install(
    FILES plugin/see_code.lua
    DESTINATION share/nvim/site/plugin
    RENAME see_code.lua
)
