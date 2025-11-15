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
  void renderGrids(const RadioSystem &radioSystem, const glm::mat4 &view,
                   const glm::mat4 &projection);
  void renderFDTDGrids(const RadioSystem &radioSystem, const glm::mat4 &view,
                       const glm::mat4 &projection);
  void cleanup();

private:
  GLuint nodeShaderProgram;
  GLuint sphereVAO, sphereVBO, sphereEBO;
  size_t sphereIndexCount;

  GLuint gridShaderProgram;
  GLuint gridVAO, gridVBO;

  GLuint volumeShaderProgram;
  GLuint volumeVAO, volumeVBO;
  GLuint volumeTexture;

  void createSphere(float radius, int segments);
  void createGridBuffers();
  void createVolumeBuffers();
  GLuint compileShader(const std::string &source, GLenum type);
  GLuint createShaderProgram(const std::string &vertexSource,
                             const std::string &fragmentSource);

  static const std::string nodeVertexShader;
  static const std::string nodeFragmentShader;
  static const std::string gridVertexShader;
  static const std::string gridFragmentShader;
  static const std::string volumeVertexShader;
  static const std::string volumeFragmentShader;
};
