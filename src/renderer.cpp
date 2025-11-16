#include "renderer.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

Renderer::Renderer()
    : shaderProgram(0), VAO(0), VBO(0), EBO(0), indexCount(0) {}

Renderer::~Renderer() { cleanup(); }

char *Renderer::loadShaderSource(const char *path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Failed to open shader file: " << path << std::endl;
    return nullptr;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  char *source = new char[content.size() + 1];
  std::strcpy(source, content.c_str());

  return source;
}

bool Renderer::initialize(int width, int height) {
  char *vertexSource = loadShaderSource("shaders/scene.vert");
  char *fragmentSource = loadShaderSource("shaders/scene.frag");

  if (!vertexSource || !fragmentSource) {
    delete[] vertexSource;
    delete[] fragmentSource;
    std::cerr << "Failed to load scene shaders" << std::endl;
    return false;
  }

  shaderProgram = createShaderProgram(vertexSource, fragmentSource);

  delete[] vertexSource;
  delete[] fragmentSource;

  if (shaderProgram == 0) {
    std::cerr << "Failed to create shader program" << std::endl;
    return false;
  }

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE); // Enable MSAA

  glClearColor(0.5f, 0.7f, 1.0f, 1.0f);

  return true;
}

void Renderer::render(const glm::mat4 &view, const glm::mat4 &projection,
                      const glm::mat4 &model) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (indexCount == 0)
    return;

  glUseProgram(shaderProgram);

  GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
  GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
  GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
  GLint lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
  GLint lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
  GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");

  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
  glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);

  glUniform3f(lightPosLoc, 1000.0f, 2000.0f, 1000.0f);
  glUniform3f(lightColorLoc, 1.0f, 1.0f, 0.9f);

  glm::mat4 invView = glm::inverse(view);
  glm::vec3 cameraPos = glm::vec3(invView[3]);
  glUniform3f(viewPosLoc, cameraPos.x, cameraPos.y, cameraPos.z);

  // Visual settings uniforms
  GLint enableFogLoc = glGetUniformLocation(shaderProgram, "enableFog");
  GLint fogDensityLoc = glGetUniformLocation(shaderProgram, "fogDensity");
  GLint fogColorLoc = glGetUniformLocation(shaderProgram, "fogColor");

  glUniform1i(enableFogLoc, visualSettings.enableFog ? 1 : 0);
  glUniform1f(fogDensityLoc, visualSettings.fogDensity);
  glUniform3f(fogColorLoc, visualSettings.fogColor.x, visualSettings.fogColor.y, visualSettings.fogColor.z);

  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount),
                 GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void Renderer::setModelData(const std::vector<float> &vertices,
                            const std::vector<unsigned int> &indices) {
  indexCount = indices.size();

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);
}

void Renderer::cleanup() {
  if (VAO)
    glDeleteVertexArrays(1, &VAO);
  if (VBO)
    glDeleteBuffers(1, &VBO);
  if (EBO)
    glDeleteBuffers(1, &EBO);
  if (shaderProgram)
    glDeleteProgram(shaderProgram);
}

GLuint Renderer::compileShader(const std::string &source, GLenum type) {
  GLuint shader = glCreateShader(type);
  const char *src = source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    std::cerr << "Shader compilation error: " << infoLog << std::endl;
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint Renderer::createShaderProgram(const std::string &vertexSource,
                                     const std::string &fragmentSource) {
  GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
  GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

  if (vertexShader == 0 || fragmentShader == 0) {
    return 0;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(program, 512, nullptr, infoLog);
    std::cerr << "Program linking error: " << infoLog << std::endl;
    glDeleteProgram(program);
    program = 0;
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}