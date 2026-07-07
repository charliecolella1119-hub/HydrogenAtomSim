#include "Shader.h"

#include <iostream>

const char* vertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

out vec3 particleColor;
out float viewDepth;
out vec3 sphereCenterView;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float particleSize;
uniform float viewportHeight;
uniform int particleStyle;
uniform float raySphereGlowRadius;
uniform bool flowingParticlesMode;
uniform int magneticQuantumNumber;
uniform float flowTime;
uniform float flowSpeed;
uniform float flowTrailLength;
uniform float flowParticleFraction;

void main()
{
    vec3 animatedPosition = position;

    if (flowingParticlesMode && magneticQuantumNumber != 0)
    {
        uint particleHash = uint(gl_VertexID) * 1664525u + 1013904223u;
        float selection = float(particleHash & 65535u) / 65535.0;

        if (selection <= flowParticleFraction)
        {
        float rho = max(length(position.xy), 0.05);
        float angularVelocity =
            float(magneticQuantumNumber) * flowSpeed /
            (rho * (rho + 0.25));
        angularVelocity = clamp(angularVelocity, -8.0, 8.0);

        float maxAngle = min(flowTrailLength / rho, 0.35);
        float angleSpan = max(2.0 * maxAngle, 0.001);
        float seed = float((particleHash >> 16u) & 65535u) / 65535.0;
        float progress = flowTime * abs(angularVelocity) + seed * angleSpan;
        float angle = sign(float(magneticQuantumNumber)) *
                      (mod(progress, angleSpan) - maxAngle);
        angle *= step(0.0001, abs(flowSpeed));
        float cosine = cos(angle);
        float sine = sin(angle);
        animatedPosition.xy = mat2(cosine,  sine,
                                  -sine,    cosine) * position.xy;
        }
    }

    // World -> View
    vec4 viewPos = view * model * vec4(animatedPosition, 1.0);

    // View -> Clip
    gl_Position = projection * viewPos;

    // Store depth for fog/lighting
    viewDepth = -viewPos.z;
    sphereCenterView = viewPos.xyz;

    // Pass particle color
    particleColor = color;

    // Make particles grow when camera gets closer
    float distanceScale =
        1.0 / sqrt(max(abs(viewPos.z), 0.1));

    if (particleStyle == 3)
    {
        float sphereRadius = particleSize * 0.025;
        gl_PointSize = sphereRadius * max(raySphereGlowRadius, 1.0) *
                       projection[1][1] * viewportHeight /
                       max(-viewPos.z, 0.1);
    }
    else if (particleStyle == 4 || particleStyle == 5)
    {
        // Presentation grains behave like Kavan-style camera-scaled particles:
        // they grow naturally as the camera moves in, but keep a crisp disc.
        float grainRadius = particleSize * (particleStyle == 5 ? 0.017 : 0.020);
        gl_PointSize = grainRadius * projection[1][1] * viewportHeight /
                       max(-viewPos.z, 0.1);
    }
    else
    {
        gl_PointSize = particleSize * distanceScale * 28.0;
    }
}
)";

const char* particleFragmentShaderSource = R"(
#version 330 core

in vec3 particleColor;
in float viewDepth;
in vec3 sphereCenterView;
out vec4 FragColor;

uniform float brightness;
uniform float alphaScale;
uniform float lightingStrength;
uniform float spriteSoftness;
uniform float depthFadeStrength;
uniform float particleSize;
uniform float viewportWidth;
uniform float viewportHeight;
uniform float rayMetallic;
uniform float rayRoughness;
uniform float rayAmbient;
uniform vec3 rayLightDirection;
uniform vec3 rayKeyColor;
uniform vec3 rayFillColor;
uniform float rayKeyIntensity;
uniform float rayFillIntensity;
uniform float raySpecularStrength;
uniform float rayRimStrength;
uniform float rayEnvironmentStrength;
uniform float raySphereOpacity;
uniform float raySphereGlow;
uniform float raySphereGlowRadius;
uniform float raySphereEdgeSoftness;
uniform bool densityShapingEnabled;
uniform float densityBrightness;
uniform float densityAlphaBoost;
uniform float highlightBoost;
uniform mat4 projection;
uniform mat4 inverseProjection;

uniform int particleStyle;
uniform int colorMapMode;
// 0 = Gaussian cloud
// 1 = Lit ball
// 2 = Bright core
// 3 = Analytic ray-traced sphere
// 4 = Crisp presentation grain
// 5 = Kavan-style color-preserving grain

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosine, vec3 reflectance)
{
    float oneMinusCosine = 1.0 - cosine;
    float fresnelFactor = oneMinusCosine * oneMinusCosine;
    fresnelFactor *= fresnelFactor * oneMinusCosine;
    return reflectance +
           (vec3(1.0) - reflectance) * fresnelFactor;
}

float colorDensityCue(vec3 color)
{
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float chroma = max(color.r, max(color.g, color.b)) -
                   min(color.r, min(color.g, color.b));
    return clamp(smoothstep(0.16, 0.90, luma) +
                 smoothstep(0.12, 0.82, chroma) * 0.28,
                 0.0,
                 1.0);
}

vec3 applyDensityColorShape(vec3 color, float cue)
{
    if (!densityShapingEnabled)
        return color;

    float brightnessShape = mix(1.0,
                                mix(0.72, 1.56, cue),
                                densityBrightness);
    vec3 shaped = color * brightnessShape;

    float core = cue * cue;
    shaped += mix(color, vec3(1.0), 0.62) * core * highlightBoost;

    return shaped;
}

float applyDensityAlphaShape(float alpha, float cue)
{
    if (!densityShapingEnabled)
        return alpha;

    float alphaShape = mix(1.0,
                           mix(0.70, 1.26, cue),
                           densityAlphaBoost);
    return alpha * alphaShape;
}

void main()
{
    gl_FragDepth = gl_FragCoord.z;

    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);

    if (r2 > 1.0)
    {
        discard;
    }

    // --------------------
    // 0: Gaussian cloud
    // --------------------
    if (particleStyle == 0)
    {
        float gaussian = exp(-r2 * spriteSoftness);
        float densityCue = colorDensityCue(particleColor);

        vec3 color =
            applyDensityColorShape(particleColor, densityCue) *
            brightness *
            gaussian;

        float alpha =
            applyDensityAlphaShape(alphaScale, densityCue) *
            gaussian;

        FragColor = vec4(color, alpha);
    }

    // --------------------
    // 1: Lit ball / sphere impostor
    // --------------------
    else if (particleStyle == 1)
    {
        float densityCue = colorDensityCue(particleColor);
        float z = sqrt(1.0 - r2);

        vec3 normal = normalize(vec3(coord.x, coord.y, z));
        vec3 lightDir = normalize(vec3(-0.45, 0.55, 1.0));
        vec3 viewDir = vec3(0.0, 0.0, 1.0);

        float diffuse =
            max(dot(normal, lightDir), 0.0);

        vec3 reflectDir =
            reflect(-lightDir, normal);

        float specular =
            pow(max(dot(viewDir, reflectDir), 0.0), 24.0);

        float edgeFade =
            smoothstep(1.0, 0.65, r2);

        float shade =
            0.80 +
            diffuse * 0.30;

        vec3 color =
            applyDensityColorShape(particleColor, densityCue) *
            brightness *
            shade;

        color += vec3(1.0) * specular * 0.16;

        FragColor =
            vec4(color,
                 applyDensityAlphaShape(alphaScale, densityCue) * edgeFade);
    }

    // --------------------
    // 2: Bright core / glow sprite
    // --------------------
    else if (particleStyle == 2)
    {
        float densityCue = colorDensityCue(particleColor);
        float core =
            exp(-r2 * spriteSoftness * 2.2);

        float halo =
            exp(-r2 * spriteSoftness * 0.45);

        vec3 color =
            applyDensityColorShape(particleColor, densityCue) *
            brightness *
            (core * 2.2 + halo * 0.65);

        float alpha =
            applyDensityAlphaShape(alphaScale, densityCue) *
            clamp(core + halo * 0.45, 0.0, 1.0);

        FragColor = vec4(color, alpha);
    }

    // --------------------
    // 4: Crisp presentation grain
    // --------------------
    else if (particleStyle == 4)
    {
        float densityCue = colorDensityCue(particleColor);
        vec3 shapedParticleColor =
            applyDensityColorShape(particleColor, densityCue);

        // A lightly faceted disc reads more like discrete rendered particles
        // than a Gaussian sprite, especially when zoomed in.
        float facets = 10.0;
        float angle = atan(coord.y, coord.x) + PI;
        float sector = 2.0 * PI / facets;
        float centeredAngle = mod(angle + sector * 0.5, sector) -
                              sector * 0.5;
        float polygonRadius = cos(sector * 0.5) /
                              max(cos(centeredAngle), 0.001);
        float facetedDistance = length(coord) / polygonRadius;

        float edgeWidth = mix(0.018, 0.075, raySphereEdgeSoftness);
        float coverage = 1.0 - smoothstep(1.0 - edgeWidth,
                                          1.0 + edgeWidth,
                                          facetedDistance);

        if (coverage <= 0.001)
            discard;

        vec3 normal = normalize(vec3(coord.x * 0.42,
                                     coord.y * 0.42,
                                     1.0));
        vec3 lightDirection = normalize(rayLightDirection);
        float diffuse = clamp((dot(normal, lightDirection) + 0.38) / 1.38,
                              0.0,
                              1.0);
        float center = 1.0 - smoothstep(0.0, 1.0, facetedDistance);
        float sparkle = pow(center, 18.0) * raySpecularStrength * 0.10;
        float shade = mix(0.90, 1.08, diffuse) +
                      center * raySphereGlow * 0.08;

        vec3 color = shapedParticleColor * brightness * shade;
        color += shapedParticleColor * sparkle;

        float alpha =
            applyDensityAlphaShape(alphaScale, densityCue) *
            raySphereOpacity *
            coverage;

        FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
    }

    // --------------------
    // 5: Kavan-style color-preserving grain
    // --------------------
    else if (particleStyle == 5)
    {
        float densityCue = colorDensityCue(particleColor);

        float facets = 11.0;
        float angle = atan(coord.y, coord.x) + PI;
        float sector = 2.0 * PI / facets;
        float centeredAngle = mod(angle + sector * 0.5, sector) -
                              sector * 0.5;
        float polygonRadius = cos(sector * 0.5) /
                              max(cos(centeredAngle), 0.001);
        float facetedDistance = length(coord) / polygonRadius;

        float edgeWidth = mix(0.006, 0.030, raySphereEdgeSoftness);
        float coverage = 1.0 - smoothstep(1.0 - edgeWidth,
                                          1.0 + edgeWidth,
                                          facetedDistance);

        if (coverage <= 0.001)
            discard;

        // Keep the orbital color map dominant.  Only a tiny center lift is
        // applied so zoomed-in grains remain separate rather than foggy.
        float center = 1.0 - smoothstep(0.15, 1.0, facetedDistance);
        float densityLift = mix(0.92, 1.12, densityCue);
        float centerLift = 1.0 + center * raySphereGlow * 0.10;
        vec3 paletteColor = particleColor;

        if (colorMapMode == 0)
        {
            // Gold: use an explicit density ramp so the preset reads as
            // red/orange outskirts -> rich yellow -> warm white lobe centers.
            // Do not infer this from RGB, because Heat Map also contains
            // yellow/orange values and should keep its own mapping.
            float goldDensity = clamp(max((particleColor.g - 0.20) / 0.72,
                                          (particleColor.b - 0.02) / 0.36),
                                      0.0,
                                      1.0);
            goldDensity = max(goldDensity, densityCue * 0.62);
            goldDensity = clamp(goldDensity, 0.0, 1.0);

            vec3 outerGold = vec3(0.86, 0.16, 0.012);
            vec3 orangeGold = vec3(1.00, 0.42, 0.012);
            vec3 yellowGold = vec3(1.00, 0.78, 0.035);
            vec3 coreGold = vec3(1.00, 0.995, 0.92);

            paletteColor = mix(outerGold,
                               orangeGold,
                               smoothstep(0.00, 0.38, goldDensity));
            paletteColor = mix(paletteColor,
                               yellowGold,
                               smoothstep(0.26, 0.68, goldDensity));

            // The original Gold map's blue channel only rises strongly in
            // the dense high stop, so it is a cleaner core signal than green.
            float sourceCore = smoothstep(0.105, 0.36, particleColor.b);
            float whiteCore = smoothstep(0.44, 0.90, goldDensity) *
                              (0.30 + highlightBoost * 0.78) *
                              (0.78 + center * 0.22);
            whiteCore = max(whiteCore,
                            sourceCore * (0.42 + highlightBoost * 0.62));
            paletteColor = mix(paletteColor,
                               coreGold,
                               clamp(whiteCore, 0.0, 0.98));

            densityLift = mix(0.88, 1.22, goldDensity);
        }
        else if (colorMapMode == 2)
        {
            // Viridis / green scientific palette: high-density cores drift
            // toward the pale mint high stop instead of plain white.
            float coreStrength = highlightBoost *
                                 smoothstep(0.50, 0.92, densityCue) *
                                 (0.40 + center * 0.60);
            paletteColor = mix(particleColor,
                               vec3(0.78, 1.00, 0.76),
                               clamp(coreStrength, 0.0, 0.70));
        }
        else if (colorMapMode == 5)
        {
            // Heat Map already has the desired purple -> magenta -> orange ->
            // white ramp from Hydrogen.cpp. Preserve it exactly here.
            paletteColor = particleColor;
        }
        else
        {
            float coreStrength = highlightBoost *
                                 smoothstep(0.52, 0.94, densityCue) *
                                 (0.45 + center * 0.55);
            paletteColor = mix(particleColor,
                               vec3(1.0),
                               clamp(coreStrength, 0.0, 0.40));
        }

        vec3 color = paletteColor * brightness * densityLift * centerLift;

        float alpha = alphaScale *
                      raySphereOpacity *
                      coverage *
                      mix(0.82, 1.0, densityCue);

        FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
    }

    // --------------------
    // 3: Ray-traced sphere impostor
    // --------------------
    else if (particleStyle == 3)
    {
        float densityCue = colorDensityCue(particleColor);
        float glowRadius = max(raySphereGlowRadius, 1.0);
        float solidDiscRadius = 1.0 / glowRadius;
        float solidDiscR2 = solidDiscRadius * solidDiscRadius;
        float radial = sqrt(max(r2, 0.0));
        vec3 shapedParticleColor =
            applyDensityColorShape(particleColor, densityCue);

        if (r2 > solidDiscR2)
        {
            float haloT = (radial - solidDiscRadius) /
                          max(1.0 - solidDiscRadius, 0.001);
            float halo = exp(-haloT * haloT * mix(9.0, 3.2,
                                                  raySphereEdgeSoftness));
            halo *= 1.0 - smoothstep(solidDiscRadius, 1.0, radial);

            vec3 haloColor = shapedParticleColor * brightness *
                             raySphereGlow *
                             (0.34 + densityCue * 0.80) *
                             halo;
            float haloAlpha =
                applyDensityAlphaShape(alphaScale, densityCue) *
                raySphereOpacity *
                raySphereGlow *
                0.34 *
                halo;

            FragColor = vec4(haloColor, clamp(haloAlpha, 0.0, 0.65));
            return;
        }

        // Reconstruct a camera ray in view space and analytically intersect it
        // with this particle's sphere, mirroring the CPU path tracer's core.
        vec2 viewport = vec2(viewportWidth, viewportHeight);
        vec2 ndc = (gl_FragCoord.xy / viewport) * 2.0 - 1.0;
        vec4 viewPoint = inverseProjection * vec4(ndc, -1.0, 1.0);
        vec3 rayDirection = normalize(viewPoint.xyz / viewPoint.w);

        float sphereRadius = particleSize * 0.025;
        vec3 oc = -sphereCenterView;
        float halfB = dot(oc, rayDirection);
        float c = dot(oc, oc) - sphereRadius * sphereRadius;
        float discriminant = halfB * halfB - c;

        if (discriminant < 0.0)
            discard;

        float hitDistance = -halfB - sqrt(discriminant);
        if (hitDistance <= 0.0)
            discard;

        vec3 hitPosition = rayDirection * hitDistance;
        vec3 normal = normalize(hitPosition - sphereCenterView);
        vec3 viewDirection = normalize(-hitPosition);
        vec3 lightDirection = normalize(rayLightDirection);
        vec3 halfVector = normalize(lightDirection + viewDirection);

        vec3 fillDirection = normalize(vec3(-lightDirection.x,
                                            -0.35 * lightDirection.y,
                                             0.8));
        float roughness = clamp(rayRoughness, 0.06, 1.0);
        float nDotV = max(dot(normal, viewDirection), 0.001);
        float nDotL = max(dot(normal, lightDirection), 0.0);
        float wrappedKey = clamp((dot(normal, lightDirection) + 0.16) / 1.16,
                                 0.0,
                                 1.0);
        float wrappedFill = clamp((dot(normal, fillDirection) + 0.32) / 1.32,
                                  0.0,
                                  1.0);

        vec3 baseReflectance = mix(vec3(0.04),
                                   particleColor,
                                   rayMetallic);
        vec3 fresnel = fresnelSchlick(max(dot(halfVector, viewDirection), 0.0),
                                      baseReflectance);
        float specularExponent = mix(112.0, 7.0, roughness);
        float specularLobe = pow(max(dot(normal, halfVector), 0.0),
                                 specularExponent);
        specularLobe *= mix(1.35, 0.45, roughness);
        vec3 specular = fresnel * specularLobe;

        // Keep the sampled color map as the albedo. Lighting adds shape without
        // running the palette through a tonemapper or gamma transform, so the
        // original saturated phase, heat-map, viridis, and gold colors survive.
        float hemisphere = normal.y * 0.5 + 0.5;
        float ambientLight = mix(0.68, 0.92, rayAmbient) *
                             mix(0.88, 1.04, hemisphere);
        float lightScale = 0.55 * lightingStrength;
        vec3 directLighting =
            rayKeyColor * rayKeyIntensity * wrappedKey +
            rayFillColor * rayFillIntensity * wrappedFill;

        vec3 color = shapedParticleColor * brightness *
                     (ambientLight + lightScale * directLighting);
        color += specular * rayKeyColor * rayKeyIntensity * nDotL *
                 raySpecularStrength;

        vec3 reflectionDirection = reflect(-viewDirection, normal);
        float environmentMix = reflectionDirection.y * 0.5 + 0.5;
        vec3 environment = mix(vec3(0.16, 0.13, 0.11),
                               vec3(0.42, 0.55, 0.78),
                               environmentMix);
        vec3 environmentFresnel =
            fresnelSchlick(nDotV, baseReflectance);
        color += environment * environmentFresnel * rayEnvironmentStrength;

        float rim = pow(1.0 - nDotV, 3.0) * rayRimStrength;
        color += mix(shapedParticleColor, rayFillColor, 0.35) * rim;

        float thickness = sqrt(max(1.0 - r2 / max(solidDiscR2, 0.0001),
                                   0.0));
        float softCoverage = mix(1.0,
                                 smoothstep(0.0, 1.0, thickness),
                                 raySphereEdgeSoftness);
        float innerGlow = raySphereGlow *
                          (0.14 + densityCue * 0.32 + rim * 0.68);
        color += shapedParticleColor * brightness * innerGlow;

        // A small contrast expansion restores the crisp separation between
        // low-density and high-density colors from the original point shader.
        color = max((color - vec3(0.5)) * 1.08 + vec3(0.5), vec3(0.0));

        vec4 clipHit = projection * vec4(hitPosition, 1.0);
        gl_FragDepth = (clipHit.z / clipHit.w) * 0.5 + 0.5;
        FragColor = vec4(color,
                         clamp(applyDensityAlphaShape(alphaScale,
                                                      densityCue) *
                               raySphereOpacity *
                               softCoverage,
                               0.0,
                               1.0));
    }
}
)";

const char* volumeVertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec2 position;

out vec2 uv;

void main()
{
    uv = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

const char* volumeFragmentShaderSource = R"(
#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform int quantumN;
uniform int quantumL;
uniform int quantumM;

uniform float volumeScale;
uniform float volumeBrightness;
uniform float volumeDensity;

uniform float volumeZoom;
uniform float volumeBounds;
uniform float orbitalScale;

uniform float rotationX;
uniform float rotationY;

uniform int colorMapMode;

uniform bool sliceMode;
uniform int sliceAxis;
uniform float sliceThickness;

uniform bool clippingMode;
uniform float clippingZ;

uniform bool sphericalCutoutMode;
uniform vec3 cutoutCenter;
uniform float cutoutRadius;

uniform bool superpositionMode;

uniform int n2;
uniform int l2;
uniform int m2;

uniform float superpositionMix;
uniform float superpositionPhase;

// --------------------------
// Rotation helpers go here
// --------------------------

mat3 rotateX(float a)
{
    float c = cos(a);
    float s = sin(a);

    return mat3(
        1.0, 0.0, 0.0,
        0.0, c, -s,
        0.0, s, c
    );
}

mat3 rotateY(float a)
{
    float c = cos(a);
    float s = sin(a);

    return mat3(
         c, 0.0, s,
        0.0, 1.0, 0.0,
        -s, 0.0, c
    );
}


float factorial(int n)
{
    float result = 1.0;

    for (int i = 2; i <= n; i++)
    {
        result *= float(i);
    }

    return result;
}

float associatedLaguerre(int k, int alpha, float x)
{
    if (k == 0) return 1.0;
    if (k == 1) return 1.0 + float(alpha) - x;

    float Lkm2 = 1.0;
    float Lkm1 = 1.0 + float(alpha) - x;
    float Lk = 0.0;

    for (int i = 2; i <= 10; i++)
    {
        if (i > k) break;

        Lk = ((2.0 * float(i) - 1.0 + float(alpha) - x) * Lkm1 -
              (float(i) - 1.0 + float(alpha)) * Lkm2) / float(i);

        Lkm2 = Lkm1;
        Lkm1 = Lk;
    }

    return Lk;
}

float associatedLegendre(int l, int m, float x)
{
    m = abs(m);

    float pmm = 1.0;

    if (m > 0)
    {
        float somx2 = sqrt(max(0.0, (1.0 - x) * (1.0 + x)));
        float fact = 1.0;

        for (int i = 1; i <= 10; i++)
        {
            if (i > m) break;

            pmm *= -fact * somx2;
            fact += 2.0;
        }
    }

    if (l == m)
        return pmm;

    float pmmp1 = x * float(2 * m + 1) * pmm;

    if (l == m + 1)
        return pmmp1;

    float pll = 0.0;

    for (int ll = 2; ll <= 10; ll++)
    {
        if (ll < m + 2) continue;
        if (ll > l) break;

        pll = (float(2 * ll - 1) * x * pmmp1 -
               float(ll + m - 1) * pmm) / float(ll - m);

        pmm = pmmp1;
        pmmp1 = pll;
    }

    return pll;
}

float hydrogenPsiState(vec3 p, int n, int l, int m)
{
    float r = length(p);

    if (r < 0.0001)
    {
        r = 0.0001;
    }

    if (n < 1 || l < 0 || l >= n || abs(m) > l)
    {
        return 0.0;
    }

    float rho = 2.0 * r / float(n);

    int laguerreK = n - l - 1;
    int laguerreAlpha = 2 * l + 1;

    float radial =
        exp(-rho / 2.0) *
        pow(rho, float(l)) *
        associatedLaguerre(laguerreK, laguerreAlpha, rho);

    float thetaCos = clamp(p.z / r, -1.0, 1.0);
    float phi = atan(p.y, p.x);

    float P = associatedLegendre(l, m, thetaCos);

    float angular = 0.0;

    if (m > 0)
    {
        angular = P * cos(float(m) * phi);
    }
    else if (m < 0)
    {
        angular = P * sin(float(abs(m)) * phi);
    }
    else
    {
        angular = P;
    }

    return radial * angular;
}

float hydrogenPsi(vec3 p)
{
    return hydrogenPsiState(p, quantumN, quantumL, quantumM);
}

vec3 mixColor(vec3 a, vec3 b, float t)
{
    return a * (1.0 - t) + b * t;
}

vec3 densityColor(float density, float psi)
{
    density = sqrt(clamp(density, 0.0, 1.0));

    if (colorMapMode == 0) // Gold
    {
        vec3 low  = vec3(0.25, 0.10, 0.00);
        vec3 mid  = vec3(1.00, 0.55, 0.00);
        vec3 high = vec3(1.00, 0.95, 0.35);

        if (density < 0.5)
            return mixColor(low, mid, density / 0.5);
        else
            return mixColor(mid, high, (density - 0.5) / 0.5);
    }
    else if (colorMapMode == 1) // Violet
    {
        vec3 low  = vec3(0.035, 0.020, 0.260);
        vec3 mid  = vec3(0.420, 0.080, 0.920);
        vec3 high = vec3(0.700, 0.880, 1.000);

        if (density < 0.5)
            return mixColor(low, mid, density / 0.5);
        else
            return mixColor(mid, high, (density - 0.5) / 0.5);
    }
    else if (colorMapMode == 2) // Viridis
    {
        vec3 low  = vec3(0.05, 0.10, 0.35);
        vec3 mid  = vec3(0.00, 0.65, 0.45);
        vec3 high = vec3(0.90, 1.00, 0.35);

        if (density < 0.5)
            return mixColor(low, mid, density / 0.5);
        else
            return mixColor(mid, high, (density - 0.5) / 0.5);
    }
    else if (colorMapMode == 3) // Phase
    {
        if (psi >= 0.0)
            return vec3(0.0, density, density);
        else
            return vec3(density, 0.0, density);
    }

    return vec3(density); // White
}

void main()
{
    vec2 screen = uv * 2.0 - 1.0;

    mat3 rot = rotateY(rotationY) * rotateX(rotationX);

    vec3 rayOrigin = rot * vec3(0.0, 0.0, 4.0);
    vec3 rayDir = normalize(rot * vec3(screen.x * volumeZoom,
                                   screen.y * volumeZoom,
                                  -1.5));

    vec4 accum = vec4(0.0);

    float t = 0.0;
    float stepSize = 0.035;

    for (int i = 0; i < 160; i++)
    {
        vec3 samplePos = rayOrigin + rayDir * t;

        if (sliceMode)
        {
            float axisValue = samplePos.z;

            if (sliceAxis == 0)
                axisValue = samplePos.x;
            else if (sliceAxis == 1)
                axisValue = samplePos.y;

            if (abs(axisValue) > sliceThickness)
            {
                t += stepSize;
                continue;
            }
        }

        if (clippingMode && samplePos.z > clippingZ)
        {
            t += stepSize;
            continue;
        }

        if (sphericalCutoutMode)
        {
            if (length(samplePos - cutoutCenter) < cutoutRadius)
            {
                t += stepSize;
                continue;
            }
        }

        if (abs(samplePos.x) > volumeBounds ||
        abs(samplePos.y) > volumeBounds ||
        abs(samplePos.z) > volumeBounds)
        {
            t += stepSize;
            continue;
        }

       vec3 orbitalPos = samplePos * orbitalScale / volumeScale;

        float psi = 0.0;

        if (superpositionMode)
        {
            float psiA = hydrogenPsiState(orbitalPos, quantumN, quantumL, quantumM);
            float psiB = hydrogenPsiState(orbitalPos, n2, l2, m2);

            float mixA = sqrt(1.0 - superpositionMix);
            float mixB = sqrt(superpositionMix);

            psi = mixA * psiA + mixB * cos(superpositionPhase) * psiB;
        }
        else
        {
            psi = hydrogenPsi(orbitalPos);
        }

        float density = psi * psi;
        density *= volumeDensity;

        vec3 sampleColor =
            densityColor(density, psi) *
            density *
            volumeBrightness;

        float alpha = clamp(density * 0.04, 0.0, 0.15);

        accum.rgb += (1.0 - accum.a) * sampleColor * alpha;
        accum.a += (1.0 - accum.a) * alpha;

        t += stepSize;
    }

    FragColor = accum;
}
)";

const char* currentVertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

out vec3 lineColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    lineColor = color;
}
)";

const char* postProcessFragmentShaderSource = R"(
#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
uniform vec3 backgroundColor;

uniform bool bloomEnabled;
uniform float bloomStrength;
uniform float bloomThreshold;
uniform float bloomRadius;
uniform float postExposure;
uniform float postContrast;
uniform float postSaturation;

uniform bool ambientOcclusionEnabled;
uniform float ambientOcclusionStrength;
uniform float ambientOcclusionRadius;
uniform float ambientOcclusionBias;

uniform float nearPlane;
uniform float farPlane;

float linearDepth(float depth)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) /
           (farPlane + nearPlane - z * (farPlane - nearPlane));
}

float brightWeight(vec3 color)
{
    float peak = max(color.r, max(color.g, color.b));
    return smoothstep(bloomThreshold, bloomThreshold + 0.65, peak);
}

vec3 brightSample(vec2 sampleUv)
{
    vec3 color = texture(sceneColor, sampleUv).rgb;
    return color * brightWeight(color);
}

float depthOcclusion(vec2 sampleUv, float centerDepth)
{
    float sampleDepth = linearDepth(texture(sceneDepth, sampleUv).r);
    float depthDelta = centerDepth - sampleDepth;
    float rangeWeight = 1.0 - smoothstep(0.0, 0.28, abs(depthDelta));
    return step(ambientOcclusionBias, depthDelta) * rangeWeight;
}

void main()
{
    vec2 texel = 1.0 / viewportSize;
    vec3 baseColor = texture(sceneColor, uv).rgb;
    float centerDepthSample = texture(sceneDepth, uv).r;

    float ao = 1.0;
    if (ambientOcclusionEnabled && centerDepthSample < 0.9999)
    {
        float centerDepth = linearDepth(centerDepthSample);
        vec2 aoStep = texel * ambientOcclusionRadius;
        float occlusion =
            depthOcclusion(uv + vec2( aoStep.x, 0.0), centerDepth) +
            depthOcclusion(uv + vec2(-aoStep.x, 0.0), centerDepth) +
            depthOcclusion(uv + vec2(0.0,  aoStep.y), centerDepth) +
            depthOcclusion(uv + vec2(0.0, -aoStep.y), centerDepth);

        ao = 1.0 - ambientOcclusionStrength * (occlusion / 4.0);

        // Transparent edge particles can write depth while remaining almost
        // indistinguishable from the clear color. Do not outline that invisible
        // depth; reserve AO for visible orbital structure.
        float subjectMask = smoothstep(0.045,
                                       0.18,
                                       length(baseColor - backgroundColor));
        ao = mix(1.0, ao, subjectMask);
    }

    vec3 bloom = vec3(0.0);
    if (bloomEnabled)
    {
        vec2 bloomStep = texel * bloomRadius;
        bloom =
            brightSample(uv + vec2( bloomStep.x, 0.0)) +
            brightSample(uv + vec2(-bloomStep.x, 0.0)) +
            brightSample(uv + vec2(0.0,  bloomStep.y)) +
            brightSample(uv + vec2(0.0, -bloomStep.y));
        bloom *= 0.25;
    }

    vec3 finalColor = baseColor * ao + bloom * bloomStrength;
    finalColor *= postExposure;

    float luma = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
    finalColor = mix(vec3(luma), finalColor, postSaturation);
    finalColor = max((finalColor - vec3(0.5)) * postContrast + vec3(0.5),
                     vec3(0.0));

    FragColor = vec4(finalColor, 1.0);
}
)";

const char* currentFragmentShaderSource = R"(
#version 330 core

in vec3 lineColor;
out vec4 FragColor;

uniform float lineAlpha;

void main()
{
    FragColor = vec4(lineColor, lineAlpha);
}
)";

void checkShaderCompile(GLuint shader, const std::string& name)
{
    int success;
    char infoLog[1024];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << name << " shader compilation failed:\n"
                  << infoLog << "\n";
    }
}

void checkProgramLink(GLuint program)
{
    int success;
    char infoLog[1024];

    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success)
    {
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n"
                  << infoLog << "\n";
    }
}

GLuint createParticleShaderProgram()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    checkShaderCompile(vertexShader, "Vertex");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &particleFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    checkShaderCompile(fragmentShader, "Fragment");

    GLuint shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    glLinkProgram(shaderProgram);
    checkProgramLink(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

GLuint createVolumeShaderProgram()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &volumeVertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    checkShaderCompile(vertexShader, "Volume Vertex");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &volumeFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    checkShaderCompile(fragmentShader, "Volume Fragment");

    GLuint shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    glLinkProgram(shaderProgram);
    checkProgramLink(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

GLuint createCurrentShaderProgram()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &currentVertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    checkShaderCompile(vertexShader, "Current Vertex");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &currentFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    checkShaderCompile(fragmentShader, "Current Fragment");

    GLuint shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    glLinkProgram(shaderProgram);
    checkProgramLink(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

GLuint createPostProcessShaderProgram()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &volumeVertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    checkShaderCompile(vertexShader, "Post-process Vertex");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &postProcessFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    checkShaderCompile(fragmentShader, "Post-process Fragment");

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkProgramLink(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}
