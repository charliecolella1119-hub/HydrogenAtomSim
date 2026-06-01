cmake_minimum_required(VERSION 3.16)

project(HydrogenAtomSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)
include_directories(/opt/homebrew/include)

link_directories(/opt/homebrew/lib)

add_executable(HydrogenAtomSim
    main.cpp
    Hydrogen.cpp
    Shader.cpp
    Camera.cpp
)

target_link_libraries(HydrogenAtomSim
    glfw
    GLEW
    "-framework OpenGL"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
)
