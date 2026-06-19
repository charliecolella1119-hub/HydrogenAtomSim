#include "Hydrogen.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

const float PI = 3.1415926535f;

float randomFloat(float min, float max)
{
    return min +
           static_cast<float>(rand()) /
           static_cast<float>(RAND_MAX) *
           (max - min);
}

void validateQuantumState(QuantumState& state)
{
    int maxN = 6;

    if (state.n < 1) state.n = 1;
    if (state.n > maxN) state.n = maxN;

    if (state.l < 0) state.l = 0;
    if (state.l > state.n - 1) state.l = state.n - 1;

    if (state.m < -state.l) state.m = -state.l;
    if (state.m > state.l) state.m = state.l;

    if (state.l == 0) state.m = 0;
}

float factorial(int n)
{
    return std::tgamma(n + 1.0f);
}

float associatedLaguerre(int k, int alpha, float x)
{
    if (k == 0) return 1.0f;
    if (k == 1) return 1.0f + alpha - x;

    float Lkm2 = 1.0f;
    float Lkm1 = 1.0f + alpha - x;
    float Lk = 0.0f;

    for (int i = 2; i <= k; i++)
    {
        Lk = ((2.0f * i - 1.0f + alpha - x) * Lkm1 -
              (i - 1.0f + alpha) * Lkm2) / i;

        Lkm2 = Lkm1;
        Lkm1 = Lk;
    }

    return Lk;
}

float associatedLegendre(int l, int m, float x)
{
    m = std::abs(m);

    float pmm = 1.0f;

    if (m > 0)
    {
        float somx2 = std::sqrt((1.0f - x) * (1.0f + x));
        float fact = 1.0f;

        for (int i = 1; i <= m; i++)
        {
            pmm *= -fact * somx2;
            fact += 2.0f;
        }
    }

    if (l == m) return pmm;

    float pmmp1 = x * (2.0f * m + 1.0f) * pmm;

    if (l == m + 1) return pmmp1;

    float pll = 0.0f;

    for (int ll = m + 2; ll <= l; ll++)
    {
        pll = ((2.0f * ll - 1.0f) * x * pmmp1 -
               (ll + m - 1.0f) * pmm) / (ll - m);

        pmm = pmmp1;
        pmmp1 = pll;
    }

    return pll;
}

float hydrogenPsi(const QuantumState& state,
                  float r,
                  float theta,
                  float phi)
{
    int n = state.n;
    int l = state.l;
    int m = state.m;

    if (n < 1 || l < 0 || l >= n || std::abs(m) > l)
        return 0.0f;

    float rho = 2.0f * r / n;

    int laguerreK = n - l - 1;
    int laguerreAlpha = 2 * l + 1;

    float radial =
        std::exp(-rho / 2.0f) *
        std::pow(rho, l) *
        associatedLaguerre(laguerreK, laguerreAlpha, rho);

    float x = std::cos(theta);
    float P = associatedLegendre(l, m, x);

    float angular = 0.0f;

    if (m > 0)
        angular = P * std::cos(m * phi);
    else if (m < 0)
        angular = P * std::sin(std::abs(m) * phi);
    else
        angular = P;

    return radial * angular;
}

float getVisualScale(const QuantumState& state)
{
    if (state.n == 1) return 1.40f;
    if (state.n == 2) return 0.65f;
    if (state.n == 3) return 0.32f;
    if (state.n == 4) return 0.18f;
    if (state.n == 5) return 0.11f;
    if (state.n == 6) return 0.075f;

    return 0.25f;
}

float getMaxRadius(const QuantumState& state)
{
    if (state.n == 1) return 8.0f;
    if (state.n == 2) return 14.0f;
    if (state.n == 3) return 22.0f;
    if (state.n == 4) return 32.0f;
    if (state.n == 5) return 45.0f;
    if (state.n == 6) return 60.0f;

    return 10.0f;
}

glm::vec3 mixColor(glm::vec3 a, glm::vec3 b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    return a * (1.0f - t) + b * t;
}

glm::vec3 densityColor(float density, float psi, int colorMapMode)
{
    if (density < 0.0f) density = 0.0f;
    if (density > 1.0f) density = 1.0f;

    glm::vec3 color;

    // Gold
    if (colorMapMode == 0)
    {
        glm::vec3 low  = glm::vec3(0.45f, 0.22f, 0.02f);
        glm::vec3 mid  = glm::vec3(1.00f, 0.58f, 0.08f);
        glm::vec3 high = glm::vec3(1.00f, 0.92f, 0.38f);

        if (density < 0.6f)
            color = mixColor(low, mid, density / 0.6f);
        else
            color = mixColor(mid, high, (density - 0.6f) / 0.4f);
    }

    // Pink Plasma
    else if (colorMapMode == 1)
    {
        glm::vec3 low  = glm::vec3(0.28f, 0.02f, 0.35f);
        glm::vec3 mid  = glm::vec3(0.95f, 0.08f, 0.65f);
        glm::vec3 high = glm::vec3(0.75f, 1.00f, 0.80f);

        if (density < 0.6f)
            color = mixColor(low, mid, density / 0.6f);
        else
            color = mixColor(mid, high, (density - 0.6f) / 0.4f);
    }

    // Green Viridis
    else if (colorMapMode == 2)
    {
        glm::vec3 low  = glm::vec3(0.02f, 0.22f, 0.16f);
        glm::vec3 mid  = glm::vec3(0.00f, 0.62f, 0.36f);
        glm::vec3 high = glm::vec3(0.72f, 0.95f, 0.70f);

        if (density < 0.6f)
            color = mixColor(low, mid, density / 0.6f);
        else
            color = mixColor(mid, high, (density - 0.6f) / 0.4f);
    }

    // Phase coloring
    else if (colorMapMode == 3)
    {
        if (psi >= 0.0f)
        {
            glm::vec3 low  = glm::vec3(0.02f, 0.16f, 0.38f);
            glm::vec3 mid  = glm::vec3(0.00f, 0.55f, 0.75f);
            glm::vec3 high = glm::vec3(0.55f, 0.95f, 1.00f);

        if (density < 0.6f)
            color = mixColor(low, mid, density / 0.6f);
        else
            color = mixColor(mid, high, (density - 0.6f) / 0.4f);
        }
        else
        {
            glm::vec3 low  = glm::vec3(0.25f, 0.02f, 0.30f);
            glm::vec3 mid  = glm::vec3(0.72f, 0.05f, 0.70f);
            glm::vec3 high = glm::vec3(1.00f, 0.45f, 0.90f);

            if (density < 0.6f)
                color = mixColor(low, mid, density / 0.6f);
            else
                color = mixColor(mid, high, (density - 0.6f) / 0.4f);
        }
    }

    // White / grayscale
    else if (colorMapMode == 4)
    {
        glm::vec3 low  = glm::vec3(0.20f, 0.22f, 0.25f);
        glm::vec3 mid  = glm::vec3(0.70f, 0.74f, 0.80f);
        glm::vec3 high = glm::vec3(1.00f, 1.00f, 1.00f);

        if (density < 0.6f)
            color = mixColor(low, mid, density / 0.6f);
        else
            color = mixColor(mid, high, (density - 0.6f) / 0.4f);
    }

    // Heat Map
    else if (colorMapMode == 5)
    {
        glm::vec3 low   = glm::vec3(0.08f, 0.00f, 0.22f); // deep purple
        glm::vec3 mid1  = glm::vec3(0.35f, 0.00f, 0.65f); // violet
        glm::vec3 mid2  = glm::vec3(0.95f, 0.20f, 0.80f); // hot pink
        glm::vec3 high  = glm::vec3(1.00f, 0.80f, 0.15f); // golden yellow
        glm::vec3 white = glm::vec3(1.00f, 1.00f, 1.00f); // bright core

        if (density < 0.15f)
        {
            color = mixColor(low, low, density / 0.15f);
        }
        else if (density < 0.35f)
        {
            color = mixColor(low, mid1, density / 0.25f);
        }
        else if (density < 0.55f)
        {
            color = mixColor(mid1, mid2,
                         (density - 0.25f) / 0.30f);
        }
        else if (density < 0.85f)
        {
            color = mixColor(mid2, high,
                         (density - 0.55f) / 0.30f);
        }
        else
        {
            color = mixColor(high, white,
                         (density - 0.85f) / 0.15f);
        }
    }

    return color;

    
}

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
    float cutoutRadius)
{
    std::vector<float> data;

    float maxRadius = getMaxRadius(state);
    float maxProbability = 100.0f;
    float visualScale = getVisualScale(state);

    int attempts = 0;
    int maxAttempts = count * 1000;

    while (data.size() < static_cast<size_t>(count * 6) &&
           attempts < maxAttempts)
    {
        attempts++;

        float r = randomFloat(0.0f, maxRadius);
        float theta = std::acos(randomFloat(-1.0f, 1.0f));
        float phi = randomFloat(0.0f, 2.0f * PI);

        float x = r * std::sin(theta) * std::cos(phi);
        float y = r * std::sin(theta) * std::sin(phi);
        float z = r * std::cos(theta);

        float psi = hydrogenPsi(state, r, theta, phi);
        float probability = psi * psi;

        if (sliceMode)
        {
            float axisValue = z;

            if (sliceAxis == 0) axisValue = x;
            else if (sliceAxis == 1) axisValue = y;
            else axisValue = z;

            if (std::abs(axisValue) > sliceThickness)
                continue;
        }

        if (clippingMode && z > clippingZ)
            continue;

        if (sphericalCutoutMode)
        {
            glm::vec3 p = glm::vec3(x, y, z);

            if (glm::length(p - cutoutCenter) < cutoutRadius)
                continue;
        }

        if (randomFloat(0.0f, maxProbability) < probability)
        {
            float density = probability / maxProbability;

            if (density > 1.0f)
                density = 1.0f;

            // Softer contrast curve.
            density = std::pow(density, 0.22f);

            // Do not allow outer particles to become black.
            if (density < 0.18f)
                density = 0.18f;

            glm::vec3 color = densityColor(density, psi, colorMapMode);

            float distanceFromCenter = std::sqrt(x * x + y * y + z * z);

            // Very gentle shell variation only.
            float shell = std::sin(distanceFromCenter * 0.65f);
            float shellBoost = 0.96f + 0.04f * shell;

            color *= shellBoost;

            data.push_back(x * visualScale);
            data.push_back(y * visualScale);
            data.push_back(z * visualScale);

            data.push_back(color.r);
            data.push_back(color.g);
            data.push_back(color.b);
        }
    }

    if (data.size() < static_cast<size_t>(count * 6))
    {
        std::cout << "Warning: only generated "
                  << data.size() / 6
                  << " points for n="
                  << state.n
                  << ", l="
                  << state.l
                  << ", m="
                  << state.m
                  << "\n";
    }

    if (attempts >= maxAttempts)
    {
        std::cout << "Generation stopped early for state: "
                  << "n=" << state.n
                  << " l=" << state.l
                  << " m=" << state.m
                  << "\n";
    }

    std::cout << "Generated points: " << data.size() / 6 << "\n";

    return data;
}

float getCameraDistance(const QuantumState& state)
{
    if (state.n == 1) return 1.0f;
    if (state.n == 2) return 1.4f;
    if (state.n == 3) return 1.8f;
    if (state.n == 4) return 2.2f;
    if (state.n == 5) return 2.6f;
    if (state.n == 6) return 3.0f;

    return 2.0f;
}

float hydrogenEnergyEV(int n)
{
    return -13.6f / (n * n);
}

std::string quantumStateName(const QuantumState& state)
{
    std::string name = std::to_string(state.n);

    if (state.l == 0) name += "s";
    else if (state.l == 1) name += "p";
    else if (state.l == 2) name += "d";
    else if (state.l == 3) name += "f";
    else if (state.l == 4) name += "g";
    else if (state.l == 5) name += "h";

    return name;
}