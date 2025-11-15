#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

class RadioSystem;

class NodeRenderer {
public:
  NodeRenderer();
  ~NodeRenderer();

  bool initialize();
  void render(const RadioSystem &radioSystem, const glm::mat4 &view,
              const glm::mat4 &projection);
  void renderPlacementPreview(const glm::vec3 &position, const glm::vec3 &color,
                              const glm::mat4 &view,
                              const glm::mat4 &projection);
  void cleanup();

private:
  // Sphere rendering for nodes
  GLuint nodeShaderProgram;
  GLuint sphereVAO, sphereVBO, sphereEBO;
  size_t sphereIndexCount;

  void createSphere(float radius, int segments);
  GLuint compileShader(const std::string &source, GLenum type);
  GLuint createShaderProgram(const std::string &vertexSource,
                             const std::string &fragmentSource);

  static const std::string nodeVertexShader;
  static const std::string nodeFragmentShader;
};
