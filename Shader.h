#pragma once

#include <GL/glew.h>
#include <string>

GLuint createParticleShaderProgram();
GLuint createVolumeShaderProgram();
GLuint createCurrentShaderProgram();
GLuint createPostProcessShaderProgram();

void checkShaderCompile(GLuint shader, const std::string& name);
void checkProgramLink(GLuint program);
void checkCurrentShaderCompile(GLuint shader, const std::string& name);
