#include "Camera.h"

#include <algorithm>
#include <cmath>

float cameraDistance = 1.0f;

float rotationX = 0.0f;
float rotationY = 0.0f;

float targetCameraDistance = 1.0f;
float targetRotationX = 0.0f;
float targetRotationY = 0.0f;

bool dragging = false;
double lastMouseX = 0.0;
double lastMouseY = 0.0;

void updateCameraSmoothing(float deltaTime)
{
    float smoothing = 1.0f - std::exp(-deltaTime * 14.0f);
    cameraDistance += (targetCameraDistance - cameraDistance) * smoothing;
    rotationX += (targetRotationX - rotationX) * smoothing;
    rotationY += (targetRotationY - rotationY) * smoothing;
}

void resetCameraRotation()
{
    rotationX = 0.0f;
    rotationY = 0.0f;
    targetRotationX = 0.0f;
    targetRotationY = 0.0f;
}

void setCameraDistanceImmediate(float distance)
{
    cameraDistance = std::clamp(distance, 0.3f, 20.0f);
    targetCameraDistance = cameraDistance;
}

void scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    targetCameraDistance -= static_cast<float>(yOffset) * 0.25f;

    targetCameraDistance = std::clamp(targetCameraDistance, 0.3f, 20.0f);
}

void mouseButtonCallback(GLFWwindow* window,
                         int button,
                         int action,
                         int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            dragging = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        }
        else if (action == GLFW_RELEASE)
        {
            dragging = false;
        }
    }
}

void cursorPositionCallback(GLFWwindow* window,
                            double xpos,
                            double ypos)
{
    if (dragging)
    {
        double dx = xpos - lastMouseX;
        double dy = ypos - lastMouseY;

        targetRotationY += static_cast<float>(dx) * 0.0042f;
        targetRotationX += static_cast<float>(dy) * 0.0042f;

        lastMouseX = xpos;
        lastMouseY = ypos;
    }
}
