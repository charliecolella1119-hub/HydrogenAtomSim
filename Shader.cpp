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

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    gl_PointSize = 550.0;
    particleColor = color;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core

in vec3 particleColor;
out vec4 FragColor;

void main()
{
    vec2 coord = gl_PointCoord * 2.0 - 1.0;

    float r2 = dot(coord, coord);

    if (r2 > 1.0)
    {
        discard;
    }

    float z = sqrt(1.0 - r2);

    vec3 normal = normalize(vec3(coord.x, coord.y, z));

    vec3 lightDir = normalize(vec3(-0.4, 0.7, 1.0));

    float diffuse = max(dot(normal, lightDir), 0.0);

    float ambient = 0.25;

    float lighting = ambient + diffuse * 0.9;

    float alpha = 0.27 * z;

    FragColor = vec4(particleColor * lighting * 4.5, alpha);
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

GLuint createShaderProgram()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    checkShaderCompile(vertexShader, "Vertex");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
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
