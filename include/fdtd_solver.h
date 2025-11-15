#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

// Forward declarations
struct Triangle;
class SpatialIndex;

class FDTDSolver {
public:
  FDTDSolver();
  ~FDTDSolver();

  bool initialize(int gridSize);
  void cleanup();

  // Reinitialize with new grid size (cleans up old resources first)
  bool reinitialize(int newGridSize);

  void addEmissionSource(int x, int y, int z, float strength);
  void clearEmission();
  void update();
  void reset();

  // GPU-based geometry marking (extremely fast)
  void markGeometryGPU(const glm::vec3 &gridCenter,
                       const glm::vec3 &gridHalfSize,
                       const SpatialIndex &spatialIndex,
                       float groundLevel = 0.0f, float materialEpsilon = 50.0f);

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

  // Voxel spacing controls (meters per voxel)
  float getVoxelSpacing() const { return voxelSpacing; }
  void setVoxelSpacing(float spacing) { voxelSpacing = spacing; }

private:
  int gridSize;
  float voxelSpacing; // Meters per voxel (default 5.0)

  // Field textures
  GLuint texEx, texEy, texEz;
  GLuint texHx, texHy, texHz;

  // Material textures
  GLuint texEpsilon, texMu, texEmission;

  // Compute shader programs
  GLuint updateEProgram;
  GLuint updateHProgram;
  GLuint markGeometryProgram;

  // SSBO for triangle geometry
  GLuint triangleSSBO;

  GLuint createTexture3D(int size);
  GLuint createComputeProgram(const char *shaderPath);
  GLuint compileShader(const char *source, GLenum type);
  char *loadShaderSource(const char *path);
};
