#pragma once
#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

class Renderer {
public:
  Renderer();
  ~Renderer();

  bool initialize(int width, int height);
  void render(const glm::mat4 &view, const glm::mat4 &projection,
              const glm::mat4 &model = glm::mat4(1.0f));
  void cleanup();

  void setModelData(const std::vector<float> &vertices,
                    const std::vector<unsigned int> &indices);

private:
  GLuint shaderProgram;
  GLuint VAO, VBO, EBO;

  size_t indexCount;

  GLuint compileShader(const std::string &source, GLenum type);
  GLuint createShaderProgram(const std::string &vertexSource,
                             const std::string &fragmentSource);

  static const std::string vertexShaderSource;
  static const std::string fragmentShaderSource;
};