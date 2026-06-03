cmake_minimum_required(VERSION 3.16)

project(HydrogenAtomSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)
include_directories(external/imgui)
include_directories(external/imgui/backends)

include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

add_executable(HydrogenAtomSim
    main.cpp
    Hydrogen.cpp
    Shader.cpp
    Camera.cpp

    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

target_link_libraries(HydrogenAtomSim
    glfw
    GLEW
    "-framework OpenGL"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
)
