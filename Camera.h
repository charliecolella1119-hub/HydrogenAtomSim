#pragma once

#include <GLFW/glfw3.h>

extern float cameraDistance;
extern float rotationX;
extern float rotationY;

void updateCameraSmoothing(float deltaTime);
void resetCameraRotation();
void setCameraDistanceImmediate(float distance);

void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
