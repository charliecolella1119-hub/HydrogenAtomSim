#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <string>

struct QuantumState
{
    int n;
    int l;
    int m;
};

void validateQuantumState(QuantumState& state);

float hydrogenEnergyEV(int n);
std::string quantumStateName(const QuantumState& state);

std::vector<float> generateHydrogenOrbital(
    QuantumState state,
    int count,
    bool sliceMode,
    bool clippingMode,
    float clippingZ,
    int colorMapMode,
    int sliceAxis,
    float sliceThickness,
    bool sphericalCutoutMode,
    glm::vec3 cutoutCenter,
    float cutoutRadius
);

float getCameraDistance(const QuantumState& state);

float randomFloat(float min, float max);