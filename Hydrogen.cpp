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

    if (state.n < 1)
        state.n = 1;

    if (state.n > maxN)
        state.n = maxN;

    if (state.l < 0)
        state.l = 0;

    if (state.l > state.n - 1)
        state.l = state.n - 1;

    if (state.m < -state.l)
        state.m = -state.l;

    if (state.m > state.l)
        state.m = state.l;

    if (state.l == 0)
        state.m = 0;
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

    if (l == m)
        return pmm;

    float pmmp1 = x * (2.0f * m + 1.0f) * pmm;

    if (l == m + 1)
        return pmmp1;

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
    {
        angular = P * std::cos(m * phi);
    }
    else if (m < 0)
    {
        angular = P * std::sin(std::abs(m) * phi);
    }
    else
    {
        angular = P;
    }

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

glm::vec3 densityColor(float density, float psi)
{
    density = std::sqrt(density);

    if (density < 0.0f) density = 0.0f;
    if (density > 1.0f) density = 1.0f;

    // Low density = dark blue
    // Medium density = purple/orange
    // High density = yellow-white

    glm::vec3 low  = glm::vec3(0.05f, 0.10f, 0.45f);
    glm::vec3 mid  = glm::vec3(0.80f, 0.20f, 0.90f);
    glm::vec3 high = glm::vec3(1.00f, 0.85f, 0.25f);

    glm::vec3 color;

    if (density < 0.5f)
    {
        float t = density / 0.5f;
        color = low * (1.0f - t) + mid * t;
    }
    else
    {
        float t = (density - 0.5f) / 0.5f;
        color = mid * (1.0f - t) + high * t;
    }

    // Slight phase tint
    if (psi < 0.0f)
    {
        color = glm::vec3(color.r * 0.7f,
                          color.g * 0.8f,
                          color.b * 1.2f);
    }

    return color;
}

std::vector<float> generateHydrogenOrbital(
    QuantumState state,
    int count,
    bool sliceMode,
    bool clippingMode,
    float clippingZ)
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

       
        float sliceThickness = 0.75f;

        if (sliceMode && std::abs(z) > sliceThickness)
        {
            continue;
        }

        if (clippingMode && z > clippingZ)
        {
            continue;
        }

        if (randomFloat(0.0f, maxProbability) < probability)
        {
            float density = probability / maxProbability;

            if (density > 1.0f)
            {
                density = 1.0f;
            }

            density = std::sqrt(density);

            // minimum brightness
            if (density < 0.35f)
            {
                density = 0.35f;
            }

            glm::vec3 color = densityColor(density, psi);

            float distanceFromCenter = std::sqrt(x*x + y*y + z*z);

            float fog = 1.0f - distanceFromCenter / maxRadius;
            if (fog < 0.20f) fog = 0.20f;
            if (fog > 1.0f) fog = 1.0f;

            color *= fog;

            float shell = std::sin(distanceFromCenter * 0.9f);
            float shellBoost = 0.85f + 0.15f * shell;

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

