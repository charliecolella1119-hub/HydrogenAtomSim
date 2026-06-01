#include "Camera.h"

float cameraDistance = 1.0f;

float rotationX = 0.0f;
float rotationY = 0.0f;

bool dragging = false;
double lastMouseX = 0.0;
double lastMouseY = 0.0;

void scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    cameraDistance -= static_cast<float>(yOffset) * 0.25f;

    if (cameraDistance < 0.3f)
    {
        cameraDistance = 0.3f;
    }

    if (cameraDistance > 20.0f)
    {
        cameraDistance = 20.0f;
    }
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

        rotationY += static_cast<float>(dx) * 0.005f;
        rotationX += static_cast<float>(dy) * 0.005f;

        lastMouseX = xpos;
        lastMouseY = ypos;
    }
}
