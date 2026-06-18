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

- Hydrogen orbital visualization with quantum numbers n, l, m
- Particle rendering mode
- Volume ray marching mode
- Flowing probability-current particle mode
- Color maps
- Slicing and clipping controls
- ImGui interface

## Controls

Use the ImGui panel to adjust quantum numbers, rendering style, volume settings, color maps, and slicing tools.

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

```bash
mkdir build
cd build
cmake ..
cmake --build .
./HydrogenAtomSim
```

## Demo

### Particle Rendering Mode

https://github.com/charliecolella1119-hub/HydrogenAtomSim/raw/main/videos/particle-mode.mp4

### Volume Ray Marching Mode

https://github.com/charliecolella1119-hub/HydrogenAtomSim/raw/main/videos/volume-ray-march.mp4

## License

This project is licensed under the MIT License.

## Author

Charlie Colella

Chemistry Major | Computational Chemistry & Scientific Visualization
