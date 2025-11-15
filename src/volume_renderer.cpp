#include "volume_renderer.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

VolumeRenderer::VolumeRenderer()
    : vao(0), vbo(0), shaderProgram(0), intensityScale(20.0f), stepCount(200),
      showEmissionSource(true), showGeometryEdges(true) {}

VolumeRenderer::~VolumeRenderer() { cleanup(); }

char *VolumeRenderer::loadShaderSource(const char *path) {
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

GLuint VolumeRenderer::compileShader(const char *source, GLenum type) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
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

GLuint VolumeRenderer::createShaderProgram(const char *vertexPath,
                                           const char *fragmentPath) {
  char *vertexSource = loadShaderSource(vertexPath);
  char *fragmentSource = loadShaderSource(fragmentPath);

  if (!vertexSource || !fragmentSource) {
    delete[] vertexSource;
    delete[] fragmentSource;
    return 0;
  }

  GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
  GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

  delete[] vertexSource;
  delete[] fragmentSource;

  if (vertexShader == 0 || fragmentShader == 0) {
    if (vertexShader)
      glDeleteShader(vertexShader);
    if (fragmentShader)
      glDeleteShader(fragmentShader);
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

bool VolumeRenderer::initialize() {
  // Create fullscreen quad
  static const float quadVertices[] = {-1.0f, -1.0f, 1.0f, -1.0f,
                                       -1.0f, 1.0f,  1.0f, 1.0f};

  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

  // Load shaders
  shaderProgram =
      createShaderProgram("shaders/volume.vert", "shaders/fdtd_volume.frag");

  if (shaderProgram == 0) {
    std::cerr << "Failed to create volume rendering shader program"
              << std::endl;
    return false;
  }

  std::cout << "Volume Renderer initialized" << std::endl;
  return true;
}

void VolumeRenderer::render(GLuint fieldTexture, GLuint epsilonTexture,
                            GLuint emissionTexture, const glm::mat4 &view,
                            const glm::mat4 &projection,
                            const glm::vec3 &gridCenter, float gridHalfSize,
                            int gridSize) {
  glUseProgram(shaderProgram);

  // Set matrices
  glm::mat4 invView = glm::inverse(view);
  glm::mat4 invProj = glm::inverse(projection);

  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "invView"), 1,
                     GL_FALSE, &invView[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "invProj"), 1,
                     GL_FALSE, &invProj[0][0]);

  // Set grid parameters
  glUniform3f(glGetUniformLocation(shaderProgram, "gridCenter"), gridCenter.x,
              gridCenter.y, gridCenter.z);
  glUniform1f(glGetUniformLocation(shaderProgram, "gridHalfSize"),
              gridHalfSize);
  glUniform1i(glGetUniformLocation(shaderProgram, "gridSize"), gridSize);

  // Set visualization parameters
  glUniform1f(glGetUniformLocation(shaderProgram, "intensityScale"),
              intensityScale);
  glUniform1i(glGetUniformLocation(shaderProgram, "stepCount"), stepCount);
  glUniform1i(glGetUniformLocation(shaderProgram, "showEmissionSource"),
              showEmissionSource);
  glUniform1i(glGetUniformLocation(shaderProgram, "showGeometryEdges"),
              showGeometryEdges);

  // Bind textures
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, fieldTexture);
  glUniform1i(glGetUniformLocation(shaderProgram, "volumeTexture"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_3D, epsilonTexture);
  glUniform1i(glGetUniformLocation(shaderProgram, "epsilonTexture"), 1);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_3D, emissionTexture);
  glUniform1i(glGetUniformLocation(shaderProgram, "emissionTexture"), 2);

  // Draw quad
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
}

void VolumeRenderer::cleanup() {
  if (vao)
    glDeleteVertexArrays(1, &vao);
  if (vbo)
    glDeleteBuffers(1, &vbo);
  if (shaderProgram)
    glDeleteProgram(shaderProgram);
}
