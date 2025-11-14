cmake_minimum_required(VERSION 3.15)
project(RadioWaveVisualization)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(PkgConfig REQUIRED)

# Try to find GLEW
find_package(GLEW REQUIRED)

# Add executable
add_executable(radio_viz
    src/main.cpp
    src/renderer.cpp
    src/camera.cpp
    src/model_loader.cpp
)

# Include directories
target_include_directories(radio_viz PRIVATE
    include
    ${OPENGL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(radio_viz
    ${OPENGL_LIBRARIES}
    glfw
    GLEW::GLEW
)

# Copy resources to build directory
configure_file(${CMAKE_SOURCE_DIR}/hongkong.obj
    ${CMAKE_BINARY_DIR}/hongkong.obj COPYONLY)

# Compiler-specific options
if(MSVC)
    target_compile_definitions(radio_viz PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()