cmake_minimum_required(VERSION 3.10)
project(see_code C)

set(CMAKE_C_STANDARD 99)

# Add include directories
include_directories(include)

# Add subdirectories
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
    cjson
    pthread
)

# Install target
install(TARGETS see_code DESTINATION bin)
