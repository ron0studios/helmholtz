#pragma once
#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include "visual_settings.h"

class Renderer {
public:
  Renderer();
  ~Renderer();

  bool initialize(int width, int height);
  void render(const glm::mat4 &view, const glm::mat4 &projection,
              const glm::mat4 &model = glm::mat4(1.0f));
  void cleanup();

  void setVisualSettings(const VisualSettings &settings) { visualSettings = settings; }

  void setModelData(const std::vector<float> &vertices,
                    const std::vector<unsigned int> &indices);

private:
  GLuint shaderProgram;
  GLuint VAO, VBO, EBO;

  size_t indexCount;
  VisualSettings visualSettings;

  GLuint compileShader(const std::string &source, GLenum type);
  GLuint createShaderProgram(const std::string &vertexSource,
                             const std::string &fragmentSource);
  char *loadShaderSource(const char *path);
};