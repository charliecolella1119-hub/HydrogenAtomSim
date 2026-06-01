#pragma once

#include <GL/glew.h>
#include <string>

GLuint createShaderProgram();

void checkShaderCompile(GLuint shader, const std::string& name);
void checkProgramLink(GLuint program);
