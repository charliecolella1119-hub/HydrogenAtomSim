#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Hydrogen.h"
#include "Shader.h"
#include "Camera.h"

#include <iostream>
#include <vector>
#include <ctime>

bool sliceMode = false;
bool clippingMode = false;
float clippingZ = 0.0f;
float clippingStep = 0.10f;
int pointCount = 100000;

void uploadPointsToGPU(const std::vector<float>& points, GLuint VBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER,
                 points.size() * sizeof(float),
                 points.data(),
                 GL_STATIC_DRAW);
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

void updateAtom(GLFWwindow* window,
                std::vector<float>& points,
                GLuint VBO,
                QuantumState& state,
                int pointCount,
                bool sliceMode,
                bool clippingMode,
                float clippingZ)
{
    validateQuantumState(state);

    cameraDistance = getCameraDistance(state);

    points = generateHydrogenOrbital(state, pointCount, sliceMode, clippingMode, clippingZ);
    uploadPointsToGPU(points, VBO);

    updateWindowTitle(window, state, sliceMode, pointCount);

    std::cout << "n = " << state.n
              << ", l = " << state.l
              << ", m = " << state.m
              << ", slice = " << (sliceMode ? "ON" : "OFF")
              << "\n";
}

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

    glfwWindowHint(GLFW_OPENGL_PROFILE,
                   GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(1280,
                         720,
                         "OpenGL Particle Renderer",
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);  

    GLuint shaderProgram = createShaderProgram();

    std::vector<float> points =
        generateHydrogenOrbital(state, pointCount, sliceMode, clippingMode, clippingZ);

    updateWindowTitle(window, state, sliceMode, pointCount);

    GLuint VAO;
    GLuint VBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    uploadPointsToGPU(points, VBO);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        (void*)0
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        (void*)(3 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        static bool sKeyPressed = false;

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && !sKeyPressed)
        {
            sliceMode = !sliceMode;

            updateAtom(window,
                points,
                VBO,
                state,
                pointCount,
                sliceMode,
                clippingMode,
                clippingZ);
            uploadPointsToGPU(points, VBO);

            updateWindowTitle(window, state, sliceMode, pointCount);

            std::cout << "Slice mode: "
                    << (sliceMode ? "ON" : "OFF")
                    << "\n";

            sKeyPressed = true;
        }

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_RELEASE)
        {
            sKeyPressed = false;
        }

        static bool keyPressed = false;

        static bool cKeyPressed = false;
        static bool leftBracketPressed = false;
        static bool rightBracketPressed = false;

        // Toggle clipping mode
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !cKeyPressed)
        {
            clippingMode = !clippingMode;

            points = generateHydrogenOrbital(
                state,
                pointCount,
                sliceMode,
                clippingMode,
                clippingZ
            );

            uploadPointsToGPU(points, VBO);

            std::cout << "Clipping mode: "
                    << (clippingMode ? "ON" : "OFF")
                    << ", clippingZ = "
                    << clippingZ
                    << "\n";

            cKeyPressed = true;
        }

        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE)
        {
            cKeyPressed = false;
        }

        // Move clipping plane backward
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS &&
            !leftBracketPressed)
        {
            clippingZ -= clippingStep;

            points = generateHydrogenOrbital(
                state,
                pointCount,
                sliceMode,
                clippingMode,
                clippingZ
            );

            uploadPointsToGPU(points, VBO);

            std::cout << "clippingZ = " << clippingZ << "\n";

            leftBracketPressed = true;
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_RELEASE)
        {
            leftBracketPressed = false;
        }

        // Move clipping plane forward
        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS &&
            !rightBracketPressed)
        {
            clippingZ += clippingStep;

            points = generateHydrogenOrbital(
                state,
                pointCount,
                sliceMode,
                clippingMode,
                clippingZ
            );

            uploadPointsToGPU(points, VBO);

            std::cout << "clippingZ = " << clippingZ << "\n";

            rightBracketPressed = true;
        }

        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_RELEASE)
        {
            rightBracketPressed = false;
        }

        if (!keyPressed)
        {
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            {
                state.n++;
                updateAtom(window,
                    points,
                    VBO,
                    state,
                    pointCount,
                    sliceMode,
                    clippingMode,
                    clippingZ);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            {
                state.n--;
                updateAtom(window,
                    points,
                    VBO,
                    state,
                    pointCount,
                    sliceMode,
                    clippingMode,
                    clippingZ);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            {
                state.l++;
                updateAtom(window,
                    points,
                    VBO,
                    state,
                    pointCount,
                    sliceMode,
                    clippingMode,
                    clippingZ);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            {
                state.l--;
                updateAtom(window,
                    points,
                    VBO,
                    state,
                    pointCount,
                    sliceMode,
                    clippingMode,
                    clippingZ);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS)
            {
                state.m++;
                updateAtom(window,
                    points,
                    VBO,
                    state,
                    pointCount,
                    sliceMode,
                    clippingMode,
                    clippingZ);
                keyPressed = true;
            }
            else if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
            {
                state.m--;
                updateAtom(window,
                    points,
                    VBO,
                    state,
                    pointCount,
                    sliceMode,
                    clippingMode,
                    clippingZ);
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

        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

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

        glUniformMatrix4fv(
            glGetUniformLocation(shaderProgram, "model"),
            1,
            GL_FALSE,
            glm::value_ptr(model)
        );

        glUniformMatrix4fv(
            glGetUniformLocation(shaderProgram, "view"),
            1,
            GL_FALSE,
            glm::value_ptr(view)
        );

        glUniformMatrix4fv(
            glGetUniformLocation(shaderProgram, "projection"),
            1,
            GL_FALSE,
            glm::value_ptr(projection)
        );

        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, points.size() / 6);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

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