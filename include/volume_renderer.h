#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

class VolumeRenderer {
public:
  VolumeRenderer();
  ~VolumeRenderer();

  bool initialize();
  void cleanup();

  void render(GLuint fieldTexture, GLuint epsilonTexture,
              GLuint emissionTexture, const glm::mat4 &view,
              const glm::mat4 &projection, const glm::vec3 &gridCenter,
              float gridHalfSize, int gridSize);

  // Visualization parameters
  void setIntensityScale(float scale) { intensityScale = scale; }
  void setStepCount(int steps) { stepCount = steps; }
  void setShowEmissionSource(bool show) { showEmissionSource = show; }
  void setShowGeometryEdges(bool show) { showGeometryEdges = show; }

  float getIntensityScale() const { return intensityScale; }
  int getStepCount() const { return stepCount; }
  bool getShowEmissionSource() const { return showEmissionSource; }
  bool getShowGeometryEdges() const { return showGeometryEdges; }

private:
  GLuint vao, vbo;
  GLuint shaderProgram;

  // Visualization parameters
  float intensityScale;
  int stepCount;
  bool showEmissionSource;
  bool showGeometryEdges;

  GLuint compileShader(const char *source, GLenum type);
  GLuint createShaderProgram(const char *vertexPath, const char *fragmentPath);
  char *loadShaderSource(const char *path);
};
