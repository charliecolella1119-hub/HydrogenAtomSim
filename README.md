# Hydrogen Quantum Simulator

A real-time OpenGL visualization of hydrogen atom wavefunctions generated from quantum numbers and rendered as three-dimensional probability density clouds.

## Overview

This project visualizes solutions to the hydrogen Schrödinger equation using Monte Carlo sampling of the hydrogen wavefunction:

ψ(n,l,m)

The resulting probability density

|ψ|²

is rendered as a three-dimensional particle cloud using OpenGL and GLFW.

Users can interactively explore different hydrogen orbitals by changing the principal, angular momentum, and magnetic quantum numbers.

## Features

* General hydrogen wavefunction implementation
* Support for arbitrary valid quantum states (n,l,m)
* Real-time orbital generation
* 3D camera controls
* Density-based coloring
* Slice mode visualization
* Interactive clipping planes
* Probability-density particle rendering
* OpenGL rendering pipeline

## Controls

| Key         | Action                       |
| ----------- | ---------------------------- |
| Up Arrow    | Increase n                   |
| Down Arrow  | Decrease n                   |
| Right Arrow | Increase l                   |
| Left Arrow  | Decrease l                   |
| M           | Increase m                   |
| N           | Decrease m                   |
| Mouse Drag  | Rotate camera                |
| Mouse Wheel | Zoom                         |
| S           | Toggle slice mode            |
| C           | Toggle clipping mode         |
| [           | Move clipping plane backward |
| ]           | Move clipping plane forward  |

## Physics

The simulator samples electron probability density from the hydrogen atom wavefunction

ψ(n,l,m)

which is separated into radial and angular components:

ψ(r,θ,φ)=R(r)Y(θ,φ)

Accepted samples are rendered as particles, creating a three-dimensional visualization of the electron probability distribution.

## Technologies

* C++
* OpenGL
* GLFW
* GLEW
* GLM

## Future Work

* Dear ImGui interface
* Quantum number sliders
* Volume ray marching
* Isosurface rendering
* GPU-based sampling
* Additional atomic systems
* Multi-electron atoms
* Browser/WebGL version

## Build Instructions

### Requirements

This project is currently built and tested on macOS using Homebrew.

Install the required libraries:

```bash
brew install glfw glew glm
```
### macOS Build
```
g++ -std=c++17 main.cpp Hydrogen.cpp Shader.cpp Camera.cpp -o main \
-I/opt/homebrew/include \
-L/opt/homebrew/lib \
-lglfw \
-lGLEW \
-framework OpenGL \
-framework Cocoa \
-framework IOKit \
-framework CoreVideo
```
### run 

./main

## Demo

| Image 1 | Image 2 |
|---|---|
| ![image1](screenshots/1.png) | ![image2](screenshots/2.png) |

| Image 3 | Image 4 |
|---|---|
| ![image3](screenshots/3.png) | ![image4](screenshots/4.png) |

| Image 5 |
|---|
| ![image5](screenshots/5.png) |

## License

This project is licensed under the MIT License.

## Author

Charlie Colella

Chemistry Major | Computational Chemistry & Scientific Visualization
