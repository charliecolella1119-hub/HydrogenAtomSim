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
#include <algorithm>
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

int sliceAxis = 2.0f;
float sliceThickness = 0.75f;

// Box clipping
bool boxClipMode = true;

glm::vec3 boxClipSize = glm::vec3(8.0f, 8.0f, 8.0f);
glm::vec3 boxClipCenter = boxClipSize * 0.5f;

// Sampling
int pointCount = 750000;
int requestedPointCount = pointCount;

// Particle rendering
int particleStyle = 3;
// 0 = Gaussian cloud
// 1 = Lit ball/ sphere impostor
// 2 = Bright core
// 3 = Analytic ray-traced sphere impostor
// 4 = Crisp presentation grain
// 5 = Kavan-style color-preserving grain

float particleSize = 0.8f;
float ballParticleSize = 0.62f;

float brightness = 1.4f;
float alphaScale = 0.55f;
float lightingStrength = 1.6f;
float depthFadeStrength = 8.8f;
float spriteSoftness = 6.0f;

float rayMetallic = 0.10f;
float rayRoughness = 0.32f;
float rayAmbient = 0.90f;
glm::vec3 rayLightDirection = glm::vec3(-0.45f, 0.65f, 1.0f);
glm::vec3 rayKeyColor = glm::vec3(1.0f, 0.92f, 0.80f);
glm::vec3 rayFillColor = glm::vec3(0.48f, 0.68f, 1.0f);
float rayKeyIntensity = 0.68f;
float rayFillIntensity = 0.22f;
float raySpecularStrength = 0.42f;
float rayRimStrength = 0.14f;
float rayEnvironmentStrength = 0.18f;
float raySphereOpacity = 0.38f;
float raySphereGlow = 0.36f;
float raySphereGlowRadius = 1.65f;
float raySphereEdgeSoftness = 0.72f;
bool raySphereDepthWrite = false;

bool additiveBlend = false;

// Presentation framing
bool showPresentationCube = true;
float presentationCubeSize = 8.0f;
float presentationCubeAlpha = 0.42f;

// Density / cinematic particle shaping
bool densityShapingEnabled = false;
float densityBrightness = 0.45f;
float densityAlphaBoost = 0.28f;
float highlightBoost = 0.18f;

// HDR post-processing
bool postProcessingEnabled = true;
bool bloomEnabled = true;
float bloomStrength = 0.38f;
float bloomThreshold = 1.05f;
float bloomRadius = 4.0f;
float postExposure = 1.0f;
float postContrast = 1.08f;
float postSaturation = 1.06f;

bool ambientOcclusionEnabled = true;
float ambientOcclusionStrength = 0.30f;
float ambientOcclusionRadius = 3.0f;
float ambientOcclusionBias = 0.008f;

// Flowing probability-current particles
bool flowingParticlesMode = true;

float flowSpeed = 0.4f;
float flowTrailLength = 0.1f;
float flowParticleFraction = 0.25f;

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


// Color / visual style
glm::vec3 backgroundColor = glm::vec3(0.94f, 0.95f, 0.94f);

int colorMapMode = 2;
// 0 = Gold
// 1 = Violet
// 2 = Viridis
// 3 = Phase
// 4 = White
// 5 = Heat Map

// --------------------
// Helpers
// --------------------

void uploadPointsToGPU(const std::vector<float>& points, GLuint VBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER,
                 points.size() * sizeof(float),
                 points.data(),
                 GL_STATIC_DRAW);
}

bool resizePostProcessTargets(GLuint& framebuffer,
                              GLuint& colorTexture,
                              GLuint& depthTexture,
                              int width,
                              int height,
                              int& currentWidth,
                              int& currentHeight)
{
    if (framebuffer != 0 && width == currentWidth && height == currentHeight)
        return true;

    if (framebuffer == 0)
    {
        glGenFramebuffers(1, &framebuffer);
        glGenTextures(1, &colorTexture);
        glGenTextures(1, &depthTexture);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA16F,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           colorTexture,
                           0);

    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_DEPTH_COMPONENT24,
                 width,
                 height,
                 0,
                 GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_INT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D,
                           depthTexture,
                           0);

    bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    currentWidth = width;
    currentHeight = height;

    if (!complete)
        std::cout << "HDR framebuffer is incomplete\n";

    return complete;
}

void updateWindowTitle(GLFWwindow* window,
                       const QuantumState& state,
                       bool sliceMode,
                       int pointCount)
{
    std::string title =
        "Hydrogen Quantum Simulator Enhanced | " +
        quantumStateName(state) +
        " | n=" + std::to_string(state.n) +
        " l=" + std::to_string(state.l) +
        " m=" + std::to_string(state.m) +
        " | E=" + std::to_string(hydrogenEnergyEV(state.n)) + " eV" +
        " | Slice=" + (sliceMode ? "ON" : "OFF") +
        " | Points=" + std::to_string(pointCount);

    glfwSetWindowTitle(window, title.c_str());
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

    setCameraDistanceImmediate(getCameraDistance(state));

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

void applyCinematicHeatPreset()
{
    pointCount = 380000;
    requestedPointCount = pointCount;
    particleStyle = 5;
    particleSize = 1.18f;
    ballParticleSize = 1.12f;
    brightness = 0.96f;
    alphaScale = 0.58f;
    lightingStrength = 1.10f;
    spriteSoftness = 1.0f;
    rayMetallic = 0.03f;
    rayRoughness = 0.46f;
    rayAmbient = 0.82f;
    rayKeyIntensity = 0.58f;
    rayFillIntensity = 0.18f;
    raySpecularStrength = 0.24f;
    rayRimStrength = 0.30f;
    rayEnvironmentStrength = 0.18f;
    raySphereOpacity = 0.62f;
    raySphereGlow = 0.10f;
    raySphereGlowRadius = 1.0f;
    raySphereEdgeSoftness = 0.08f;
    raySphereDepthWrite = false;
    densityShapingEnabled = false;
    densityBrightness = 0.10f;
    densityAlphaBoost = 0.00f;
    highlightBoost = 0.00f;
    colorMapMode = 5;
    backgroundColor = glm::vec3(0.005f, 0.008f, 0.010f);
    postProcessingEnabled = true;
    bloomEnabled = true;
    bloomStrength = 0.08f;
    bloomThreshold = 1.75f;
    bloomRadius = 2.8f;
    postExposure = 0.92f;
    postContrast = 1.24f;
    postSaturation = 1.30f;
    ambientOcclusionEnabled = true;
    ambientOcclusionStrength = 0.18f;
    additiveBlend = false;
}

void applyCleanViridisPreset()
{
    pointCount = 430000;
    requestedPointCount = pointCount;
    particleStyle = 5;
    particleSize = 1.02f;
    ballParticleSize = 0.74f;
    brightness = 1.18f;
    alphaScale = 0.56f;
    lightingStrength = 1.05f;
    spriteSoftness = 6.6f;
    rayMetallic = 0.04f;
    rayRoughness = 0.42f;
    rayAmbient = 0.82f;
    rayKeyIntensity = 0.54f;
    rayFillIntensity = 0.16f;
    raySpecularStrength = 0.20f;
    rayRimStrength = 0.08f;
    rayEnvironmentStrength = 0.10f;
    raySphereOpacity = 0.54f;
    raySphereGlow = 0.12f;
    raySphereGlowRadius = 1.0f;
    raySphereEdgeSoftness = 0.07f;
    raySphereDepthWrite = false;
    densityShapingEnabled = false;
    densityBrightness = 0.08f;
    densityAlphaBoost = 0.00f;
    highlightBoost = 0.48f;
    colorMapMode = 2;
    backgroundColor = glm::vec3(0.455f, 0.463f, 0.447f);
    postProcessingEnabled = true;
    bloomEnabled = true;
    bloomStrength = 0.10f;
    bloomThreshold = 1.32f;
    bloomRadius = 2.8f;
    postExposure = 1.08f;
    postContrast = 1.18f;
    postSaturation = 1.32f;
    ambientOcclusionEnabled = true;
    ambientOcclusionStrength = 0.14f;
    additiveBlend = false;
}

void applyScientificGoldPreset()
{
    pointCount = 390000;
    requestedPointCount = pointCount;
    particleStyle = 5;
    particleSize = 1.10f;
    ballParticleSize = 0.76f;
    brightness = 0.83f;
    alphaScale = 0.47f;
    lightingStrength = 0.98f;
    spriteSoftness = 6.55f;
    rayMetallic = 0.10f;
    rayRoughness = 0.44f;
    rayAmbient = 0.76f;
    rayKeyIntensity = 0.56f;
    rayFillIntensity = 0.10f;
    raySpecularStrength = 0.18f;
    rayRimStrength = 0.05f;
    rayEnvironmentStrength = 0.08f;
    raySphereOpacity = 0.56f;
    raySphereGlow = 0.11f;
    raySphereGlowRadius = 1.0f;
    raySphereEdgeSoftness = 0.06f;
    raySphereDepthWrite = false;
    densityShapingEnabled = false;
    densityBrightness = 0.06f;
    densityAlphaBoost = 0.00f;
    highlightBoost = 1.08f;
    colorMapMode = 0;
    backgroundColor = glm::vec3(0.455f, 0.463f, 0.447f);
    postProcessingEnabled = true;
    bloomEnabled = true;
    bloomStrength = 0.09f;
    bloomThreshold = 1.34f;
    bloomRadius = 2.6f;
    postExposure = 1.10f;
    postContrast = 1.22f;
    postSaturation = 1.24f;
    ambientOcclusionEnabled = true;
    ambientOcclusionStrength = 0.12f;
    additiveBlend = false;
}

void applyPresentationDarkPreset()
{
    pointCount = 340000;
    requestedPointCount = pointCount;
    particleStyle = 5;
    particleSize = 1.14f;
    ballParticleSize = 0.86f;
    brightness = 1.08f;
    alphaScale = 0.56f;
    lightingStrength = 1.04f;
    spriteSoftness = 1.0f;
    rayMetallic = 0.04f;
    rayRoughness = 0.46f;
    rayAmbient = 0.84f;
    rayKeyIntensity = 0.52f;
    rayFillIntensity = 0.22f;
    raySpecularStrength = 0.22f;
    rayRimStrength = 0.30f;
    rayEnvironmentStrength = 0.16f;
    raySphereOpacity = 0.54f;
    raySphereGlow = 0.14f;
    raySphereGlowRadius = 1.0f;
    raySphereEdgeSoftness = 0.18f;
    raySphereDepthWrite = false;
    densityShapingEnabled = false;
    densityBrightness = 0.08f;
    densityAlphaBoost = 0.00f;
    highlightBoost = 0.00f;
    colorMapMode = 1;
    backgroundColor = glm::vec3(0.010f, 0.010f, 0.014f);
    postProcessingEnabled = true;
    bloomEnabled = true;
    bloomStrength = 0.14f;
    bloomThreshold = 1.44f;
    bloomRadius = 3.2f;
    postExposure = 1.20f;
    postContrast = 1.26f;
    postSaturation = 1.40f;
    ambientOcclusionEnabled = true;
    ambientOcclusionStrength = 0.16f;
    additiveBlend = false;
}

// --------------------
// Main
// --------------------

int main()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    QuantumState state = {4, 3, 0};

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
                         "Hydrogen Quantum Simulator Enhanced",
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
    GLuint currentShaderProgram = createCurrentShaderProgram();
    GLuint postProcessShaderProgram = createPostProcessShaderProgram();

    setCameraDistanceImmediate(getCameraDistance(state));

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

    points = applyBoxClipFilter(points);

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
    // Presentation cube
    // --------------------

    float c = presentationCubeSize * 0.5f;
    float cubeColor = 0.72f;
    float cubeVertices[] =
    {
        -c, -c, -c, cubeColor, cubeColor, cubeColor,
         c, -c, -c, cubeColor, cubeColor, cubeColor,
         c, -c, -c, cubeColor, cubeColor, cubeColor,
         c,  c, -c, cubeColor, cubeColor, cubeColor,
         c,  c, -c, cubeColor, cubeColor, cubeColor,
        -c,  c, -c, cubeColor, cubeColor, cubeColor,
        -c,  c, -c, cubeColor, cubeColor, cubeColor,
        -c, -c, -c, cubeColor, cubeColor, cubeColor,

        -c, -c,  c, cubeColor, cubeColor, cubeColor,
         c, -c,  c, cubeColor, cubeColor, cubeColor,
         c, -c,  c, cubeColor, cubeColor, cubeColor,
         c,  c,  c, cubeColor, cubeColor, cubeColor,
         c,  c,  c, cubeColor, cubeColor, cubeColor,
        -c,  c,  c, cubeColor, cubeColor, cubeColor,
        -c,  c,  c, cubeColor, cubeColor, cubeColor,
        -c, -c,  c, cubeColor, cubeColor, cubeColor,

        -c, -c, -c, cubeColor, cubeColor, cubeColor,
        -c, -c,  c, cubeColor, cubeColor, cubeColor,
         c, -c, -c, cubeColor, cubeColor, cubeColor,
         c, -c,  c, cubeColor, cubeColor, cubeColor,
         c,  c, -c, cubeColor, cubeColor, cubeColor,
         c,  c,  c, cubeColor, cubeColor, cubeColor,
        -c,  c, -c, cubeColor, cubeColor, cubeColor,
        -c,  c,  c, cubeColor, cubeColor, cubeColor
    };

    GLuint cubeVAO;
    GLuint cubeVBO;

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(cubeVertices),
                 cubeVertices,
                 GL_STATIC_DRAW);
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

    GLuint sceneFramebuffer = 0;
    GLuint sceneColorTexture = 0;
    GLuint sceneDepthTexture = 0;
    int sceneFramebufferWidth = 0;
    int sceneFramebufferHeight = 0;

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

    // --------------------
    // Main Loop
    // --------------------

    float previousFrameTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        float currentFrameTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrameTime - previousFrameTime;
        previousFrameTime = currentFrameTime;
        updateCameraSmoothing(deltaTime);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        // --------------------
        // ImGui Panel
        // --------------------

        bool stateChanged = false;
        
        ImGui::SetNextWindowSize(ImVec2(380, 720), ImGuiCond_Once);
        ImGui::Begin("Hydrogen Controls");
        ImGui::PushItemWidth(-135.0f);

        const char* renderModes[] =
        {
            "Particles",
            "Volume Ray March"
        };

        if (ImGui::Button("Reset View"))
        {
            resetCameraRotation();
        }

        ImGui::SameLine();
        ImGui::Text("%.1f FPS", io.Framerate);

        ImGui::Combo("Render Mode",
                     &renderMode,
                     renderModes,
                     IM_ARRAYSIZE(renderModes));

        if (ImGui::CollapsingHeader("Quick Setup", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Look presets");

            if (ImGui::Button("Cinematic Heat"))
            {
                applyCinematicHeatPreset();
                stateChanged = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Clean Viridis"))
            {
                applyCleanViridisPreset();
                stateChanged = true;
            }

            if (ImGui::Button("Scientific Gold"))
            {
                applyScientificGoldPreset();
                stateChanged = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Electric Violet"))
            {
                applyPresentationDarkPreset();
                stateChanged = true;
            }

            ImGui::SeparatorText("Common tweaks");
            ImGui::SliderFloat("Particle Size", &particleSize, 0.25f, 3.0f);
            ImGui::SliderFloat("Opacity", &raySphereOpacity, 0.02f, 1.0f);
            ImGui::SliderFloat("Sphere Glow", &raySphereGlow, 0.0f, 2.0f);
            ImGui::SliderFloat("Bloom", &bloomStrength, 0.0f, 1.5f);
            ImGui::SliderFloat("Exposure", &postExposure, 0.5f, 2.0f);
        }

        if (ImGui::CollapsingHeader("View / Clipping"))
        {
            ImGui::Checkbox("Presentation Cube", &showPresentationCube);
            ImGui::SliderFloat("Cube Alpha",
                               &presentationCubeAlpha,
                               0.0f,
                               1.0f);

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
        }

        if (ImGui::CollapsingHeader("Particle Density / Scale"))
        {
            ImGui::Text("Active Points: %d", pointCount);
            ImGui::SliderInt("Target Points",
                             &requestedPointCount,
                             75000,
                             900000);

            if (ImGui::Button("Apply Point Count"))
            {
                pointCount = requestedPointCount;
                stateChanged = true;
            }

            if (ImGui::Button("Dense Detail 750k"))
            {
                pointCount = 750000;
                requestedPointCount = pointCount;
                particleSize = 0.80f;
                ballParticleSize = 0.62f;
                raySphereGlowRadius = 1.55f;
                stateChanged = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Large Grains 320k"))
            {
                pointCount = 320000;
                requestedPointCount = pointCount;
                particleSize = 1.12f;
                ballParticleSize = 0.98f;
                raySphereOpacity = 0.34f;
                raySphereGlowRadius = 1.75f;
                stateChanged = true;
            }

            if (ImGui::Button("Hero Grains 180k"))
            {
                pointCount = 180000;
                requestedPointCount = pointCount;
                particleSize = 1.45f;
                ballParticleSize = 1.22f;
                raySphereOpacity = 0.30f;
                raySphereGlowRadius = 1.95f;
                stateChanged = true;
            }

            ImGui::TextWrapped("Lower counts with larger translucent spheres usually read closer to the reference renders and also reduce GPU load.");
        }

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

        if (ImGui::CollapsingHeader("Color"))
        {
            const char* colorMaps[] =
            {
                "Gold",
                "Violet",
                "Viridis",
                "Phase",
                "White",
                "Heat Map"
            };

            stateChanged |= ImGui::Combo("Color Map",
                                         &colorMapMode,
                                         colorMaps,
                                         IM_ARRAYSIZE(colorMaps));

            ImGui::ColorEdit3("Background", &backgroundColor.x);
        }

        if (ImGui::CollapsingHeader("Particle Rendering / Material"))
        {
            const char* particleStyles[] =
            {
                "Gaussian",
                "Lit Ball",
                "Bright Core",
                "Ray Traced Sphere",
                "Crisp Grain",
                "Kavan Grain"
            };             

            ImGui::Combo("Particle Style",
                &particleStyle,
                particleStyles,
                IM_ARRAYSIZE(particleStyles));

            ImGui::SliderFloat("Particle Size", &particleSize, 0.01f, 15.0f);
            ImGui::SliderFloat("Brightness", &brightness, 0.05f, 10.0f);
            ImGui::SliderFloat("Alpha", &alphaScale, 0.01f, 2.0f);
            ImGui::SliderFloat("Glow", &lightingStrength, 0.1f, 10.0f);
            ImGui::SliderFloat("Depth Fade", &depthFadeStrength, 0.0f, 4.0f);
            ImGui::SliderFloat("Sprite Softness", &spriteSoftness, 0.5f, 12.0f);
            ImGui::Checkbox("Additive Blending", &additiveBlend);

            ImGui::SeparatorText("Density Shaping");
            ImGui::Checkbox("Density Shaping", &densityShapingEnabled);
            ImGui::SliderFloat("Density Brightness",
                               &densityBrightness,
                               0.0f,
                               1.5f);
            ImGui::SliderFloat("Density Alpha",
                               &densityAlphaBoost,
                               0.0f,
                               1.0f);
            ImGui::SliderFloat("Core Highlight",
                               &highlightBoost,
                               0.0f,
                               1.5f);

            if (particleStyle == 3 || particleStyle == 4 || particleStyle == 5)
            {
                ImGui::SeparatorText(particleStyle == 5
                                         ? "Kavan Grain"
                                         : "Ray-Traced Material");
                if (particleStyle == 3)
                {
                    ImGui::SliderFloat("Metallic", &rayMetallic, 0.0f, 1.0f);
                    ImGui::SliderFloat("Roughness", &rayRoughness, 0.02f, 1.0f);
                    ImGui::SliderFloat("Ambient", &rayAmbient, 0.0f, 1.0f);
                }
                if (particleStyle != 5)
                {
                    ImGui::SliderFloat3("Light Direction",
                                        &rayLightDirection.x,
                                        -1.0f,
                                        1.0f);
                    ImGui::ColorEdit3("Key Light Color", &rayKeyColor.x);
                    ImGui::SliderFloat("Key Intensity",
                                       &rayKeyIntensity,
                                       0.0f,
                                       2.0f);
                    ImGui::ColorEdit3("Fill Light Color", &rayFillColor.x);
                    ImGui::SliderFloat("Fill Intensity",
                                       &rayFillIntensity,
                                       0.0f,
                                       1.0f);
                    ImGui::SliderFloat("Specular Strength",
                                       &raySpecularStrength,
                                       0.0f,
                                       2.0f);
                    ImGui::SliderFloat("Rim Strength",
                                       &rayRimStrength,
                                       0.0f,
                                       1.0f);
                    ImGui::SliderFloat("Environment",
                                       &rayEnvironmentStrength,
                                       0.0f,
                                       1.0f);
                }
                ImGui::SeparatorText("Translucency / Glow");
                ImGui::SliderFloat("Sphere Opacity",
                                   &raySphereOpacity,
                                   0.02f,
                                   1.0f);
                ImGui::SliderFloat("Sphere Glow",
                                   &raySphereGlow,
                                   0.0f,
                                   2.0f);
                if (particleStyle == 3)
                {
                    ImGui::SliderFloat("Glow Radius",
                                       &raySphereGlowRadius,
                                       1.0f,
                                       3.0f);
                }
                ImGui::SliderFloat("Edge Softness",
                                   &raySphereEdgeSoftness,
                                   0.0f,
                                   1.0f);
                if (particleStyle == 3)
                {
                    ImGui::Checkbox("Sphere Depth Writes", &raySphereDepthWrite);
                    ImGui::TextWrapped("Depth writes OFF makes ray spheres behave more like a translucent luminous cloud; ON gives more solid marble-like particles.");
                }
                else
                {
                    ImGui::TextWrapped(particleStyle == 5
                        ? "Kavan Grain preserves the color map and uses camera-scaled hard grains for cleaner close-up detail."
                        : "Crisp Grain uses camera-scaled faceted particles for cleaner close-up detail.");
                }
            }
        }

        if (ImGui::CollapsingHeader("Flowing Particles"))
        {
            ImGui::Checkbox("Flowing Probability Particles", &flowingParticlesMode);

            ImGui::SliderFloat("Flow Speed",
                               &flowSpeed,
                               0.0f,
                               5.0f);

            ImGui::SliderFloat("Flow Trail Length",
                               &flowTrailLength,
                               0.01f,
                               0.5f);

            ImGui::SliderFloat("Animated Fraction",
                               &flowParticleFraction,
                               0.05f,
                               1.0f,
                               "%.2f");

            ImGui::SliderFloat("Ball Particle Size",
                               &ballParticleSize,
                               0.05f,
                               5.0f);

            if (state.m == 0)
            {
                ImGui::Text("m = 0: no azimuthal probability current");
            }
            else
            {
                ImGui::Text("GPU probability-current animation");
            }

            ImGui::Text("Performance: %.1f FPS", io.Framerate);
        }

        if (ImGui::CollapsingHeader("Visual Particle Presets"))
        {
            if (ImGui::Button("Cinematic Heat"))
            {
                applyCinematicHeatPreset();
                stateChanged = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Clean Viridis"))
            {
                applyCleanViridisPreset();
                stateChanged = true;
            }

            if (ImGui::Button("Scientific Gold"))
            {
                applyScientificGoldPreset();
                stateChanged = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Presentation Dark"))
            {
                applyPresentationDarkPreset();
                stateChanged = true;
            }
        }

        if (ImGui::CollapsingHeader("HDR Post Processing"))
        {
            ImGui::Checkbox("Enable Post Processing", &postProcessingEnabled);
            ImGui::Checkbox("Bloom", &bloomEnabled);
            ImGui::SliderFloat("Bloom Strength", &bloomStrength, 0.0f, 2.0f);
            ImGui::SliderFloat("Bloom Threshold", &bloomThreshold, 0.5f, 3.0f);
            ImGui::SliderFloat("Bloom Radius", &bloomRadius, 1.0f, 12.0f);
            ImGui::SliderFloat("Exposure", &postExposure, 0.5f, 2.0f);
            ImGui::SliderFloat("Contrast", &postContrast, 0.75f, 1.6f);
            ImGui::SliderFloat("Saturation", &postSaturation, 0.0f, 1.8f);

            ImGui::SeparatorText("Ambient Occlusion");
            ImGui::Checkbox("Ambient Occlusion", &ambientOcclusionEnabled);
            ImGui::SliderFloat("AO Strength",
                               &ambientOcclusionStrength,
                               0.0f,
                               1.0f);
            ImGui::SliderFloat("AO Radius",
                               &ambientOcclusionRadius,
                               1.0f,
                               16.0f);
            ImGui::SliderFloat("AO Bias",
                               &ambientOcclusionBias,
                               0.001f,
                               0.05f,
                               "%.3f");
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
        
        if (ImGui::CollapsingHeader("Manual Slicing / Clipping"))
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

        ImGui::PopItemWidth();
        ImGui::End();

        if (stateChanged)
        {
            updateAtom(window, points, VBO, state);
        }

        // --------------------
        // Render Scene
        // --------------------

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        framebufferWidth = std::max(framebufferWidth, 1);
        framebufferHeight = std::max(framebufferHeight, 1);
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        if (postProcessingEnabled)
        {
            resizePostProcessTargets(sceneFramebuffer,
                                     sceneColorTexture,
                                     sceneDepthTexture,
                                     framebufferWidth,
                                     framebufferHeight,
                                     sceneFramebufferWidth,
                                     sceneFramebufferHeight);
            glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        glClearColor(backgroundColor.x,
                     backgroundColor.y,
                     backgroundColor.z,
                     1.0f);

        // Depth clears obey the write mask. The post pass disables depth
        // writes, so restore them before clearing the offscreen depth texture.
        glDepthMask(GL_TRUE);
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
                             static_cast<float>(framebufferWidth) /
                                 static_cast<float>(framebufferHeight),
                             0.1f,
                             100.0f);
        
        if (particleStyle == 1 || (particleStyle == 3 && raySphereDepthWrite))
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
            if (particleStyle == 1 || (particleStyle == 3 && raySphereDepthWrite))
            {
                glDepthMask(GL_TRUE);
            }
            else
            {
                glDepthMask(GL_FALSE);
            }

            glUseProgram(particleShaderProgram);

            float sizeToUse =
                flowingParticlesMode && state.m != 0
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

            glUniform1f(glGetUniformLocation(particleShaderProgram, "viewportWidth"),
                        static_cast<float>(framebufferWidth));

            glUniform1f(glGetUniformLocation(particleShaderProgram, "viewportHeight"),
                        static_cast<float>(framebufferHeight));

            glUniform1f(glGetUniformLocation(particleShaderProgram, "rayMetallic"),
                        rayMetallic);

            glUniform1f(glGetUniformLocation(particleShaderProgram, "rayRoughness"),
                        rayRoughness);

            glUniform1f(glGetUniformLocation(particleShaderProgram, "rayAmbient"),
                        rayAmbient);

            glUniform3fv(glGetUniformLocation(particleShaderProgram, "rayLightDirection"),
                         1,
                         glm::value_ptr(rayLightDirection));
            glUniform3fv(glGetUniformLocation(particleShaderProgram, "rayKeyColor"),
                         1,
                         glm::value_ptr(rayKeyColor));
            glUniform3fv(glGetUniformLocation(particleShaderProgram, "rayFillColor"),
                         1,
                         glm::value_ptr(rayFillColor));
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "rayKeyIntensity"),
                        rayKeyIntensity);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "rayFillIntensity"),
                        rayFillIntensity);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "raySpecularStrength"),
                        raySpecularStrength);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "rayRimStrength"),
                        rayRimStrength);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "rayEnvironmentStrength"),
                        rayEnvironmentStrength);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "raySphereOpacity"),
                        raySphereOpacity);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "raySphereGlow"),
                        raySphereGlow);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "raySphereGlowRadius"),
                        raySphereGlowRadius);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "raySphereEdgeSoftness"),
                        raySphereEdgeSoftness);
            glUniform1i(glGetUniformLocation(particleShaderProgram,
                                             "densityShapingEnabled"),
                        densityShapingEnabled);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "densityBrightness"),
                        densityBrightness);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "densityAlphaBoost"),
                        densityAlphaBoost);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "highlightBoost"),
                        highlightBoost);

            glUniform1i(glGetUniformLocation(particleShaderProgram,
                                             "particleStyle"),
                        particleStyle);
            glUniform1i(glGetUniformLocation(particleShaderProgram,
                                             "colorMapMode"),
                        colorMapMode);

            glUniform1i(glGetUniformLocation(particleShaderProgram,
                                             "flowingParticlesMode"),
                        flowingParticlesMode);
            glUniform1i(glGetUniformLocation(particleShaderProgram,
                                             "magneticQuantumNumber"),
                        state.m);
            glUniform1f(glGetUniformLocation(particleShaderProgram, "flowTime"),
                        currentFrameTime);
            glUniform1f(glGetUniformLocation(particleShaderProgram, "flowSpeed"),
                        flowSpeed);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "flowTrailLength"),
                        flowTrailLength);
            glUniform1f(glGetUniformLocation(particleShaderProgram,
                                             "flowParticleFraction"),
                        flowParticleFraction);

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

            glm::mat4 inverseProjection = glm::inverse(projection);
            glUniformMatrix4fv(
                glGetUniformLocation(particleShaderProgram, "inverseProjection"),
                1,
                GL_FALSE,
                glm::value_ptr(inverseProjection)
            );

            glBindVertexArray(VAO);

            glDrawArrays(GL_POINTS,
                         0,
                         points.size() / 6);

            glBindVertexArray(0);

            if (showPresentationCube)
            {
                glUseProgram(currentShaderProgram);
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glLineWidth(1.0f);

                glUniform1f(glGetUniformLocation(currentShaderProgram,
                                                 "lineAlpha"),
                            presentationCubeAlpha);
                glUniformMatrix4fv(
                    glGetUniformLocation(currentShaderProgram, "model"),
                    1,
                    GL_FALSE,
                    glm::value_ptr(model)
                );
                glUniformMatrix4fv(
                    glGetUniformLocation(currentShaderProgram, "view"),
                    1,
                    GL_FALSE,
                    glm::value_ptr(view)
                );
                glUniformMatrix4fv(
                    glGetUniformLocation(currentShaderProgram, "projection"),
                    1,
                    GL_FALSE,
                    glm::value_ptr(projection)
                );

                glBindVertexArray(cubeVAO);
                glDrawArrays(GL_LINES, 0, 24);
                glBindVertexArray(0);

                glEnable(GL_DEPTH_TEST);
            }
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

            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
        }

        if (postProcessingEnabled)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, framebufferWidth, framebufferHeight);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);

            glUseProgram(postProcessShaderProgram);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
            glUniform1i(glGetUniformLocation(postProcessShaderProgram, "sceneColor"), 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
            glUniform1i(glGetUniformLocation(postProcessShaderProgram, "sceneDepth"), 1);

            glUniform2f(glGetUniformLocation(postProcessShaderProgram, "viewportSize"),
                        static_cast<float>(framebufferWidth),
                        static_cast<float>(framebufferHeight));
            glUniform3fv(glGetUniformLocation(postProcessShaderProgram, "backgroundColor"),
                         1,
                         glm::value_ptr(backgroundColor));
            glUniform1i(glGetUniformLocation(postProcessShaderProgram, "bloomEnabled"),
                        bloomEnabled);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram, "bloomStrength"),
                        bloomStrength);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram, "bloomThreshold"),
                        bloomThreshold);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram, "bloomRadius"),
                        bloomRadius);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram, "postExposure"),
                        postExposure);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram, "postContrast"),
                        postContrast);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram, "postSaturation"),
                        postSaturation);
            glUniform1i(glGetUniformLocation(postProcessShaderProgram,
                                             "ambientOcclusionEnabled"),
                        ambientOcclusionEnabled && renderMode == 0);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram,
                                             "ambientOcclusionStrength"),
                        ambientOcclusionStrength);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram,
                                             "ambientOcclusionRadius"),
                        ambientOcclusionRadius);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram,
                                             "ambientOcclusionBias"),
                        ambientOcclusionBias);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram, "nearPlane"),
                        0.1f);
            glUniform1f(glGetUniformLocation(postProcessShaderProgram, "farPlane"),
                        100.0f);

            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glActiveTexture(GL_TEXTURE0);
            glEnable(GL_BLEND);
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

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);

    glDeleteProgram(particleShaderProgram);
    glDeleteProgram(volumeShaderProgram);
    glDeleteProgram(currentShaderProgram);
    glDeleteProgram(postProcessShaderProgram);

    glDeleteFramebuffers(1, &sceneFramebuffer);
    glDeleteTextures(1, &sceneColorTexture);
    glDeleteTextures(1, &sceneDepthTexture);

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

// portfolio website: https://charliecolella1119-hub.github.io/Charlie-Portfolio/
