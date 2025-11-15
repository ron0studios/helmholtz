#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

class RadioSystem;
struct SignalRay;

class PropagationRenderer {
public:
  PropagationRenderer();
  ~PropagationRenderer();

  bool initialize();
  void render(const glm::mat4 &view, const glm::mat4 &projection);
  void updateRayBuffers(const std::vector<SignalRay> &rays);
  void cleanup();

private:
  GLuint shaderProgram;
  GLuint VAO, VBO;

  struct RaySegment {
    int startIndex;
    int vertexCount;
  };

  std::vector<float> vertexData;
  std::vector<RaySegment> raySegments;

  GLuint compileShader(const std::string &source, GLenum type);
  GLuint createShaderProgram(const std::string &vertexSource,
                             const std::string &fragmentSource);

  static const std::string vertexShaderSource;
  static const std::string fragmentShaderSource;
};
