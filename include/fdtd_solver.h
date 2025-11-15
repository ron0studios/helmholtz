#pragma once

#include <GL/glew.h>

class FDTDSolver {
public:
  FDTDSolver();
  ~FDTDSolver();

  bool initialize(int gridSize);
  void cleanup();

  void addEmissionSource(int x, int y, int z, float strength);
  void clearEmission();
  void update();
  void reset();

  // Getters for textures (for rendering)
  GLuint getExTexture() const { return texEx; }
  GLuint getEyTexture() const { return texEy; }
  GLuint getEzTexture() const { return texEz; }
  GLuint getHxTexture() const { return texHx; }
  GLuint getHyTexture() const { return texHy; }
  GLuint getHzTexture() const { return texHz; }
  GLuint getEpsilonTexture() const { return texEpsilon; }
  GLuint getMuTexture() const { return texMu; }
  GLuint getEmissionTexture() const { return texEmission; }

  int getGridSize() const { return gridSize; }

private:
  int gridSize;

  // Field textures
  GLuint texEx, texEy, texEz;
  GLuint texHx, texHy, texHz;

  // Material textures
  GLuint texEpsilon, texMu, texEmission;

  // Compute shader programs
  GLuint updateEProgram;
  GLuint updateHProgram;

  GLuint createTexture3D(int size);
  GLuint createComputeProgram(const char *shaderPath);
  GLuint compileShader(const char *source, GLenum type);
  char *loadShaderSource(const char *path);
};
