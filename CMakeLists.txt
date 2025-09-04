# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(see_code C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Source directory
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add subdirectory for src libraries
add_subdirectory(src)

# Create executable
add_executable(see_code
    src/core/main.c
)

# Link libraries
target_link_libraries(see_code
    see_code_core
    see_code_gui
    see_code_network
    see_code_data
    see_code_utils
    GLESv2
    EGL
    pthread
    dl
)

# Install target
install(TARGETS see_code DESTINATION bin)
install(
    FILES plugin/see_code.lua
    DESTINATION share/nvim/site/plugin
    RENAME see_code.lua
)
