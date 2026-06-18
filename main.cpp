#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Hydrogen.h"
#include "Shader.h"
#include "Camera.h"

#include <iostream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cmath>

// --------------------
// Global Settings
// --------------------

// Slicing / clipping
bool sliceMode = false;
bool clippingMode = false;
float clippingZ = 0.0f;

bool sphericalCutoutMode = false;
float cutoutRadius = 3.0f;
glm::vec3 cutoutCenter = glm::vec3(0.0f);

int sliceAxis = 2;
float sliceThickness = 0.75f;

// Box clipping
bool boxClipMode = false;

glm::vec3 boxClipCenter = glm::vec3(0.0f);
glm::vec3 boxClipSize = glm::vec3(8.0f, 8.0f, 8.0f);

// Sampling
int pointCount = 500000;

// Particle rendering
int particleStyle = 1;
// 0 = Gaussian
// 1 = Lit Ball / Sphere Impostor
// 2 = Bright Core

float particleSize = 3.0f;
float ballParticleSize = 3.0f;

float brightness = 1.25f;
float alphaScale = 0.65f;
float lightingStrength = 1.6f;
float depthFadeStrength = 2.8f;
float spriteSoftness = 1.4f;

bool additiveBlend = false;

// Flowing probability-current particles
bool flowingParticlesMode = false;
std::vector<float> flowingPoints;

float flowSpeed = 0.8f;
float flowConfinement = 0.08f;
float flowRespawnDistance = 1.5f;

// Render modes
int renderMode = 0;
// 0 = Particles
// 1 = Volume Ray March

// Volume ray marching
float volumeScale = 2.0f;
float volumeBrightness = 4.0f;
float volumeDensity = 1.0f;
float volumeZoom = 1.5f;
float volumeBounds = 4.0f;
float volumeAutoScale = 1.0f;

// Superposition
bool superpositionMode = false;
int n2 = 2;
int l2 = 1;
int m2 = 0;
float superpositionMix = 0.5f;
float superpositionSpeed = 1.0f;

// Color / visual style
glm::vec3 backgroundColor = glm::vec3(0.92f, 0.94f, 0.94f);

int colorMapMode = 2;
// 0 = Gold
// 1 = Plasma
// 2 = Viridis
// 3 = Phase
// 4 = White

// --------------------
// Helpers
// --------------------

void uploadPointsToGPU(const std::vector<float>& points, GLuint VBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER,
                 points.size() * sizeof(float),
                 points.data(),
                 GL_DYNAMIC_DRAW);
}

void updateWindowTitle(GLFWwindow* window,
                       const QuantumState& state,
                       bool sliceMode,
                       int pointCount)
{
    std::string title =
        "Hydrogen Quantum Simulator | " +
        quantumStateName(state) +
        " | n=" + std::to_string(state.n) +
        " l=" + std::to_string(state.l) +
        " m=" + std::to_string(state.m) +
        " | E=" + std::to_string(hydrogenEnergyEV(state.n)) + " eV" +
        " | Slice=" + (sliceMode ? "ON" : "OFF") +
        " | Points=" + std::to_string(pointCount);

    glfwSetWindowTitle(window, title.c_str());
}

void initializeFlowingParticles(const std::vector<float>& points)
{
    flowingPoints = points;
}

glm::vec3 probabilityCurrentVelocity(const glm::vec3& p,
                                     const QuantumState& state)
{
    if (state.m == 0)
        return glm::vec3(0.0f);

    float rho = std::sqrt(p.x * p.x + p.y * p.y);

    if (rho < 0.05f)
        rho = 0.05f;

    glm::vec3 phiHat =
        glm::normalize(glm::vec3(-p.y, p.x, 0.0f));

    float m = static_cast<float>(state.m);

    return phiHat * (m / (rho + 0.25f));
}

void updateFlowingParticles(float dt,
                            const QuantumState& state,
                            const std::vector<float>& referencePoints)
{
    if (flowingPoints.empty() || referencePoints.empty())
        return;

    for (size_t i = 0; i + 5 < flowingPoints.size(); i += 6)
    {
        glm::vec3 p(flowingPoints[i],
                    flowingPoints[i + 1],
                    flowingPoints[i + 2]);

        glm::vec3 original(referencePoints[i],
                           referencePoints[i + 1],
                           referencePoints[i + 2]);

        glm::vec3 currentVelocity =
            probabilityCurrentVelocity(p, state) * flowSpeed;

        glm::vec3 confinementVelocity =
            (original - p) * flowConfinement;

        p += (currentVelocity + confinementVelocity) * dt;

        if (glm::length(p - original) > flowRespawnDistance)
        {
            p = original;
        }

        flowingPoints[i]     = p.x;
        flowingPoints[i + 1] = p.y;
        flowingPoints[i + 2] = p.z;
    }
}

std::vector<float> applyBoxClipFilter(const std::vector<float>& inputPoints)
{
    if (!boxClipMode)
    {
        return inputPoints;
    }

    std::vector<float> filtered;
    filtered.reserve(inputPoints.size());

    glm::vec3 halfSize = boxClipSize * 0.5f;

    glm::vec3 minCorner = boxClipCenter - halfSize;
    glm::vec3 maxCorner = boxClipCenter + halfSize;

    for (size_t i = 0; i + 5 < inputPoints.size(); i += 6)
    {
        glm::vec3 p(inputPoints[i],
                    inputPoints[i + 1],
                    inputPoints[i + 2]);

        bool inside =
            p.x >= minCorner.x && p.x <= maxCorner.x &&
            p.y >= minCorner.y && p.y <= maxCorner.y &&
            p.z >= minCorner.z && p.z <= maxCorner.z;

        if (!inside)
        {
            filtered.push_back(inputPoints[i]);
            filtered.push_back(inputPoints[i + 1]);
            filtered.push_back(inputPoints[i + 2]);
            filtered.push_back(inputPoints[i + 3]);
            filtered.push_back(inputPoints[i + 4]);
            filtered.push_back(inputPoints[i + 5]);
        }
    }

    return filtered;
}

void alignBoxCornerToOrigin()
{
    boxClipCenter = boxClipSize * 0.5f;
}

void updateAtom(GLFWwindow* window,
                std::vector<float>& points,
                GLuint VBO,
                QuantumState& state)
{
    validateQuantumState(state);

    cameraDistance = getCameraDistance(state);

    std::vector<float> generatedPoints = generateHydrogenOrbital(
    state,
    pointCount,
    sliceMode,
    clippingMode,
    clippingZ,
    colorMapMode,
    sliceAxis,
    sliceThickness,
    sphericalCutoutMode,
    cutoutCenter,
    cutoutRadius
    );

    points = applyBoxClipFilter(generatedPoints);

    initializeFlowingParticles(points);
    uploadPointsToGPU(points, VBO);
    updateWindowTitle(window, state, sliceMode, pointCount);
}

float getVolumeOrbitalScale(const QuantumState& state)
{
    if (state.n == 1) return 1.0f;
    if (state.n == 2) return 1.8f;
    if (state.n == 3) return 3.0f;
    if (state.n == 4) return 4.5f;
    if (state.n == 5) return 6.5f;
    if (state.n == 6) return 9.0f;

    return 1.0f;
}

// --------------------
// Main
// --------------------

int main()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    QuantumState state = {1, 0, 0};

    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW\n";
        return -1;
    }


    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(1280,
                         720,
                         "Hydrogen Quantum Simulator",
                         nullptr,
                         nullptr);

    if (!window)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);

    if (glewInit() != GLEW_OK)
    {
        std::cout << "Failed to initialize GLEW\n";
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glEnable(GL_PROGRAM_POINT_SIZE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    GLuint particleShaderProgram = createParticleShaderProgram();
    GLuint volumeShaderProgram = createVolumeShaderProgram();

    std::vector<float> points = generateHydrogenOrbital(
        state,
        pointCount,
        sliceMode,
        clippingMode,
        clippingZ,
        colorMapMode,
        sliceAxis,
        sliceThickness,
        sphericalCutoutMode,
        cutoutCenter,
        cutoutRadius
    );

    initializeFlowingParticles(points);
    updateWindowTitle(window, state, sliceMode, pointCount);

    // --------------------
    // Particle VAO/VBO
    // --------------------

    GLuint VAO;
    GLuint VBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    uploadPointsToGPU(points, VBO);

    glVertexAttribPointer(0,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          6 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          6 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // --------------------
    // Fullscreen Quad
    // --------------------

    float quadVertices[] =
    {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,

        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    GLuint quadVAO;
    GLuint quadVBO;

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);

    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(quadVertices),
                 quadVertices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          2 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    float lastFrameTime = static_cast<float>(glfwGetTime());

    // --------------------
    // Main Loop
    // --------------------

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        float currentFrameTime = static_cast<float>(glfwGetTime());
        float dt = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        // --------------------
        // Keyboard Controls
        // --------------------

        static bool keyPressed = false;

        if (!keyPressed)
        {
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            {
                state.n++;
                updateAtom(window, points, VBO, state);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            {
                state.n--;
                updateAtom(window, points, VBO, state);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            {
                state.l++;
                updateAtom(window, points, VBO, state);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            {
                state.l--;
                updateAtom(window, points, VBO, state);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS)
            {
                state.m++;
                updateAtom(window, points, VBO, state);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
            {
                state.m--;
                updateAtom(window, points, VBO, state);
                keyPressed = true;
            }
        }
    

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_RELEASE &&
            glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_RELEASE &&
            glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_RELEASE &&
            glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_RELEASE &&
            glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE &&
            glfwGetKey(window, GLFW_KEY_N) == GLFW_RELEASE)
        {
            keyPressed = false;
        }
    

        // --------------------
        // ImGui Panel
        // --------------------

        bool stateChanged = false;

        ImGui::Begin("Hydrogen Controls");

        const char* renderModes[] =
        {
            "Particles",
            "Volume Ray March"
        };

        if (ImGui::Button("Reset View"))
        {
            rotationX = 0.0f;
            rotationY = 0.0f;
        }

        if (ImGui::Button("Front View"))
        {
            rotationX = 0.0f;
            rotationY = 0.0f;
        }

        ImGui::SameLine();

        if (ImGui::Button("Side View"))
        {
            rotationX = 0.0f;
            rotationY = glm::radians(90.0f);
        }

        ImGui::SameLine();

        if (ImGui::Button("Top View"))
        {
            rotationX = glm::radians(90.0f);
            rotationY = 0.0f;
        }

        ImGui::Combo("Render Mode",
                     &renderMode,
                     renderModes,
                     IM_ARRAYSIZE(renderModes));

        if (ImGui::CollapsingHeader("Quantum State", ImGuiTreeNodeFlags_DefaultOpen))
        {
            stateChanged |= ImGui::SliderInt("n", &state.n, 1, 6);
            validateQuantumState(state);

            stateChanged |= ImGui::SliderInt("l", &state.l, 0, state.n - 1);
            validateQuantumState(state);

            stateChanged |= ImGui::SliderInt("m", &state.m, -state.l, state.l);
            validateQuantumState(state);

            ImGui::Text("State: %s", quantumStateName(state).c_str());
            ImGui::Text("Energy: %.3f eV", hydrogenEnergyEV(state.n));
        }

        if (ImGui::CollapsingHeader("Particle Rendering", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Particle Size", &particleSize, 2.0f, 150.0f);
            ImGui::SliderFloat("Brightness", &brightness, 0.1f, 10.0f);
            ImGui::SliderFloat("Alpha", &alphaScale, 0.01f, 2.0f);
            ImGui::SliderFloat("Glow", &lightingStrength, 0.1f, 10.0f);
            ImGui::SliderFloat("Depth Fade", &depthFadeStrength, 0.0f, 4.0f);
            ImGui::SliderFloat("Sprite Softness", &spriteSoftness, 0.5f, 12.0f);
            ImGui::Checkbox("Additive Blending", &additiveBlend);
        }

        if (ImGui::CollapsingHeader("Flowing Particles", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Flowing Probability Particles", &flowingParticlesMode);

            ImGui::SliderFloat("Flow Speed",
                               &flowSpeed,
                               0.0f,
                               5.0f);

            ImGui::SliderFloat("Flow Confinement",
                               &flowConfinement,
                               0.0f,
                               1.0f);

            ImGui::SliderFloat("Flow Respawn Distance",
                               &flowRespawnDistance,
                               0.1f,
                               5.0f);

            ImGui::SliderFloat("Ball Particle Size",
                               &ballParticleSize,
                               0.5f,
                               10.0f);

            if (state.m == 0)
            {
                ImGui::Text("m = 0: no azimuthal probability current");
            }
        }

        if (ImGui::CollapsingHeader("Visual Presets", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("Kavang Inspired"))
            {
                particleStyle = 1;          // Soft sphere
                particleSize = 4.0f;
                ballParticleSize = 4.0f;

                brightness = 1.15f;
                alphaScale = 0.55f;
                lightingStrength = 1.25f;
                spriteSoftness = 1.8f;

                colorMapMode = 1;           // Phase
                backgroundColor = glm::vec3(0.02f, 0.04f, 0.05f);

                additiveBlend = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("MinutePhysics Bright"))
            {
                particleStyle = 1;          // Soft sphere
                particleSize = 4.0f;
                ballParticleSize = 4.0f;

                brightness = 1.35f;
                alphaScale = 0.65f;
                lightingStrength = 1.45f;
                spriteSoftness = 1.6f;

                colorMapMode = 2;           // Viridis / green
                backgroundColor = glm::vec3(0.94f, 0.95f, 0.94f);

                additiveBlend = false;
            }

            if (ImGui::Button("Soft Scientific"))
            {
                particleStyle = 1;          // Bright core / glow
                particleSize = 4.0f;
                ballParticleSize = 4.0f;

                brightness = 1.28f;
                alphaScale = 0.6f;
                lightingStrength = 1.3f;
                spriteSoftness = 1.8f;

                colorMapMode = 0;           // Gold
                backgroundColor = glm::vec3(0.94f, 0.95f, 0.94f);

                additiveBlend = true;
            }
        }

        if (ImGui::CollapsingHeader("Volume Ray Marching"))
        {
            ImGui::SliderFloat("Volume Scale", &volumeScale, 0.5f, 8.0f);
            ImGui::SliderFloat("Volume Auto Scale", &volumeAutoScale, 0.2f, 3.0f);
            ImGui::SliderFloat("Volume Zoom", &volumeZoom, 0.25f, 8.0f);
            ImGui::SliderFloat("Volume Brightness", &volumeBrightness, 0.1f, 20.0f);
            ImGui::SliderFloat("Volume Density", &volumeDensity, 0.1f, 10.0f);
            ImGui::SliderFloat("Volume Bounds", &volumeBounds, 1.0f, 12.0f);
        }

        if (ImGui::CollapsingHeader("Color"))
        {
            const char* colorMaps[] =
            {
                "Gold",
                "Plasma",
                "Viridis",
                "Phase",
                "White"
            };

            stateChanged |= ImGui::Combo("Color Map",
                                         &colorMapMode,
                                         colorMaps,
                                         IM_ARRAYSIZE(colorMaps));

            ImGui::ColorEdit3("Background", &backgroundColor.x);
        }

       bool previousBoxClipMode = boxClipMode;

        if (ImGui::Checkbox("Box Clip Mode", &boxClipMode))
        {
            stateChanged = true;

            if (boxClipMode && !previousBoxClipMode)
            {
                alignBoxCornerToOrigin();
            }
        }

        if (boxClipMode)
        {
            if (ImGui::SliderFloat3("Box Size",
                            &boxClipSize.x,
                            0.5f,
                            30.0f))
            {
                alignBoxCornerToOrigin();
                stateChanged = true;
            }
        

            stateChanged |= ImGui::SliderFloat3("Box Center",
                                        &boxClipCenter.x,
                                        -20.0f,
                                        20.0f);
        }
        
        if (ImGui::CollapsingHeader("Slicing / Clipping"))
        {
            stateChanged |= ImGui::Checkbox("Slice Mode", &sliceMode);

            const char* sliceAxes[] =
            {
                "X Axis",
                "Y Axis",
                "Z Axis"
            };

            stateChanged |= ImGui::Combo("Slice Axis",
                                         &sliceAxis,
                                         sliceAxes,
                                         IM_ARRAYSIZE(sliceAxes));

            stateChanged |= ImGui::SliderFloat("Slice Thickness",
                                               &sliceThickness,
                                               0.05f,
                                               5.0f);

            stateChanged |= ImGui::Checkbox("Flat Clipping",
                                            &clippingMode);

            stateChanged |= ImGui::SliderFloat("Flat Clip Z",
                                               &clippingZ,
                                               -20.0f,
                                               20.0f);

            stateChanged |= ImGui::Checkbox("Spherical Cutout",
                                            &sphericalCutoutMode);

            stateChanged |= ImGui::SliderFloat("Cutout Radius",
                                               &cutoutRadius,
                                               0.0f,
                                               20.0f);

            stateChanged |= ImGui::SliderFloat3("Cutout Center",
                                                &cutoutCenter.x,
                                                -20.0f,
                                                20.0f);
        }

        ImGui::End();

        if (stateChanged)
        {
            updateAtom(window, points, VBO, state);
        }

        // --------------------
        // Render Scene
        // --------------------

        glClearColor(backgroundColor.x,
                     backgroundColor.y,
                     backgroundColor.z,
                     1.0f);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 model = glm::mat4(1.0f);

        model = glm::rotate(model,
                            rotationY,
                            glm::vec3(0.0f, 1.0f, 0.0f));

        model = glm::rotate(model,
                            rotationX,
                            glm::vec3(1.0f, 0.0f, 0.0f));

        glm::mat4 view =
            glm::lookAt(glm::vec3(0.0f, 0.0f, cameraDistance),
                        glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 projection =
            glm::perspective(glm::radians(45.0f),
                             1280.0f / 720.0f,
                             0.1f,
                             100.0f);

        const char* particleStyles[] =
        {
            "Gaussian",
            "Soft Sphere",
            "Bright Core"
        };

        ImGui::Combo("Particle Style",
             &particleStyle,
             particleStyles,
             IM_ARRAYSIZE(particleStyles));

        if (particleStyle == 1)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else
        {
            glDepthMask(GL_FALSE);

            if (additiveBlend)
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            else
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        if (renderMode == 0)
        {
            glEnable(GL_DEPTH_TEST);
            if (particleStyle == 1)
            {
                glDepthMask(GL_TRUE);
            }
            else
            {
                glDepthMask(GL_FALSE);
            }

            glUseProgram(particleShaderProgram);

            float sizeToUse =
                flowingParticlesMode
                ? ballParticleSize
                : particleSize;

            glUniform1f(glGetUniformLocation(particleShaderProgram, "particleSize"),
                        sizeToUse);

            glUniform1f(glGetUniformLocation(particleShaderProgram, "brightness"),
                        brightness);

            glUniform1f(glGetUniformLocation(particleShaderProgram, "alphaScale"),
                        alphaScale);

            glUniform1f(glGetUniformLocation(particleShaderProgram, "lightingStrength"),
                        lightingStrength);

            glUniform1f(glGetUniformLocation(particleShaderProgram, "depthFadeStrength"),
                        depthFadeStrength);

            glUniform1f(glGetUniformLocation(particleShaderProgram, "spriteSoftness"),
                        spriteSoftness);

            glUniform1i(glGetUniformLocation(particleShaderProgram, "particleStyle"),
            particleStyle);

            glUniformMatrix4fv(
                glGetUniformLocation(particleShaderProgram, "model"),
                1,
                GL_FALSE,
                glm::value_ptr(model)
            );

            glUniformMatrix4fv(
                glGetUniformLocation(particleShaderProgram, "view"),
                1,
                GL_FALSE,
                glm::value_ptr(view)
            );

            glUniformMatrix4fv(
                glGetUniformLocation(particleShaderProgram, "projection"),
                1,
                GL_FALSE,
                glm::value_ptr(projection)
            );

            glBindVertexArray(VAO);

            if (flowingParticlesMode && state.m != 0)
            {
                updateFlowingParticles(dt, state, points);
                uploadPointsToGPU(flowingPoints, VBO);

                glDrawArrays(GL_POINTS,
                             0,
                             flowingPoints.size() / 6);
            }
            else
            {
                uploadPointsToGPU(points, VBO);

                glDrawArrays(GL_POINTS,
                             0,
                             points.size() / 6);
            }

            glBindVertexArray(0);
        }
        else if (renderMode == 1)
        {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);

            glUseProgram(volumeShaderProgram);

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "quantumN"),
                        state.n);

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "quantumL"),
                        state.l);

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "quantumM"),
                        state.m);

            float effectiveVolumeScale =
                volumeScale / static_cast<float>(state.n);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "volumeScale"),
                        effectiveVolumeScale);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "volumeZoom"),
                        volumeZoom);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "volumeBounds"),
                        volumeBounds);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "volumeBrightness"),
                        volumeBrightness);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "volumeDensity"),
                        volumeDensity);

            float orbitalScale =
                volumeAutoScale * getVolumeOrbitalScale(state);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "orbitalScale"),
                        orbitalScale);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "rotationX"),
                        rotationX);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "rotationY"),
                        rotationY);

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "colorMapMode"),
                        colorMapMode);

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "sliceMode"),
                        sliceMode);

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "sliceAxis"),
                        sliceAxis);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "sliceThickness"),
                        sliceThickness);

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "clippingMode"),
                        clippingMode);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "clippingZ"),
                        clippingZ);

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "sphericalCutoutMode"),
                        sphericalCutoutMode);

            glUniform3f(glGetUniformLocation(volumeShaderProgram, "cutoutCenter"),
                        cutoutCenter.x,
                        cutoutCenter.y,
                        cutoutCenter.z);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "cutoutRadius"),
                        cutoutRadius);

            float phase =
                static_cast<float>(glfwGetTime()) *
                superpositionSpeed;

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "superpositionMode"),
                        superpositionMode);

            glUniform1i(glGetUniformLocation(volumeShaderProgram, "n2"), n2);
            glUniform1i(glGetUniformLocation(volumeShaderProgram, "l2"), l2);
            glUniform1i(glGetUniformLocation(volumeShaderProgram, "m2"), m2);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "superpositionMix"),
                        superpositionMix);

            glUniform1f(glGetUniformLocation(volumeShaderProgram, "superpositionPhase"),
                        phase);

            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
        }

        // --------------------
        // Render ImGui
        // --------------------

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --------------------
    // Cleanup
    // --------------------

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);

    glDeleteProgram(particleShaderProgram);
    glDeleteProgram(volumeShaderProgram);

    glfwTerminate();

    return 0;
}

/*------------------------------------------------------------------------
 g++ -std=c++17 main.cpp Hydrogen.cpp Shader.cpp Camera.cpp -o main \
-I/opt/homebrew/include \
-L/opt/homebrew/lib \
-lglfw \
-lGLEW \
-framework OpenGL \
-framework Cocoa \
-framework IOKit \
-framework CoreVideo
-------------------------------------------------------------------------*/

/*--------------------
mkdir build
cd build
cmake ..
cmake --build .
./HydrogenAtomSim
--------------------*/

//git commit --amend --reset-author
//git push --set-upstream origin imgui-ui
