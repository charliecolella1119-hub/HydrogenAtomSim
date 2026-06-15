#include "Shader.h"

#include <iostream>

const char* vertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

out vec3 particleColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float particleSize;

void main()
{
    gl_Position =
        projection * view * model * vec4(position, 1.0);

    gl_PointSize = particleSize;
    particleColor = color;
}
)";

const char* particleFragmentShaderSource = R"(
#version 330 core

in vec3 particleColor;
out vec4 FragColor;

uniform float brightness;
uniform float alphaScale;
uniform float lightingStrength;
uniform float spriteSoftness;

void main()
{
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);

    if (r2 > 1.0)
        discard;

    float z = sqrt(1.0 - r2);
    vec3 normal = normalize(vec3(coord.x, coord.y, z));

    vec3 lightDir = normalize(vec3(-0.4, 0.6, 1.0));

    float diffuse = max(dot(normal, lightDir), 0.0);
    float rim = pow(1.0 - z, 2.0);
    float gaussian = exp(-r2 * spriteSoftness);

    vec3 shadedColor =
        particleColor *
        brightness *
        (0.25 + diffuse * lightingStrength);

    shadedColor += particleColor * rim * 0.8;

    float alpha = alphaScale * gaussian;

    FragColor = vec4(shadedColor, alpha);
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
    else if (colorMapMode == 1) // Plasma
    {
        vec3 low  = vec3(0.05, 0.00, 0.25);
        vec3 mid  = vec3(0.85, 0.10, 0.85);
        vec3 high = vec3(1.00, 0.85, 0.10);

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

const char* currentFragmentShaderSource = R"(
#version 330 core

in vec3 lineColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(lineColor, 0.75);
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
        std::cout << name << " shader compilation failed:\n"
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
        std::cout << "Shader program linking failed:\n"
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