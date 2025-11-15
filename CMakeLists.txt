cmake_minimum_required(VERSION 3.15)
project(RadioWaveVisualization)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenMP REQUIRED)

# Try to find GLEW
find_package(GLEW REQUIRED)

# ImGui source files
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Add executable
add_executable(radio_viz
    src/main.cpp
    src/renderer.cpp
    src/camera.cpp
    src/model_loader.cpp
    src/radio_system.cpp
    src/spatial_index.cpp
    src/node_manager.cpp
    src/node_renderer.cpp
    src/ui_manager.cpp
    src/fdtd_solver.cpp
    ${IMGUI_SOURCES}
)

# Include directories
target_include_directories(radio_viz PRIVATE
    include
    ${OPENGL_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Link libraries
target_link_libraries(radio_viz
    ${OPENGL_LIBRARIES}
    glfw
    GLEW::GLEW
	OpenMP::OpenMP_CXX
)

# Copy resources to build directory
configure_file(${CMAKE_SOURCE_DIR}/hongkong.obj
    ${CMAKE_BINARY_DIR}/hongkong.obj COPYONLY)

# Compiler-specific options
if(MSVC)
    target_compile_definitions(radio_viz PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
