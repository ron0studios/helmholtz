#include "fdtd_solver.h"
#include "spatial_index.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

FDTDSolver::FDTDSolver()
    : gridSize(0), voxelSpacing(5.0f), texEx(0), texEy(0), texEz(0), texHx(0), texHy(0), texHz(0),
      texEpsilon(0), texMu(0), texEmission(0), updateEProgram(0),
      updateHProgram(0), markGeometryProgram(0), triangleSSBO(0) {}

FDTDSolver::~FDTDSolver() { cleanup(); }

GLuint FDTDSolver::createTexture3D(int size) {
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_3D, tex);

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, size, size, size, 0, GL_RED, GL_FLOAT,
               nullptr);

  return tex;
}

char *FDTDSolver::loadShaderSource(const char *path) {
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

GLuint FDTDSolver::compileShader(const char *source, GLenum type) {
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

GLuint FDTDSolver::createComputeProgram(const char *shaderPath) {
  char *source = loadShaderSource(shaderPath);
  if (!source) {
    return 0;
  }

  GLuint shader = compileShader(source, GL_COMPUTE_SHADER);
  delete[] source;

  if (shader == 0) {
    return 0;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, shader);
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

  glDeleteShader(shader);
  return program;
}

bool FDTDSolver::initialize(int size) {
  gridSize = size;

  // Create field textures
  texEx = createTexture3D(gridSize);
  texEy = createTexture3D(gridSize);
  texEz = createTexture3D(gridSize);
  texHx = createTexture3D(gridSize);
  texHy = createTexture3D(gridSize);
  texHz = createTexture3D(gridSize);

  // Create material textures
  texEpsilon = createTexture3D(gridSize);
  texMu = createTexture3D(gridSize);
  texEmission = createTexture3D(gridSize);

  // Initialize epsilon and mu to 1.0 (vacuum)
  std::vector<float> data(gridSize * gridSize * gridSize, 1.0f);

  glBindTexture(GL_TEXTURE_3D, texEpsilon);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, data.data());

  glBindTexture(GL_TEXTURE_3D, texMu);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, data.data());

  // Initialize emission to 0.0
  std::fill(data.begin(), data.end(), 0.0f);
  glBindTexture(GL_TEXTURE_3D, texEmission);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, data.data());

  // Load compute shaders
  updateEProgram = createComputeProgram("shaders/fdtd_update_e.comp");
  updateHProgram = createComputeProgram("shaders/fdtd_update_h.comp");
  markGeometryProgram = createComputeProgram("shaders/mark_geometry.comp");

  if (updateEProgram == 0 || updateHProgram == 0 || markGeometryProgram == 0) {
    std::cerr << "Failed to create FDTD compute shaders" << std::endl;
    return false;
  }

  std::cout << "FDTD Solver initialized with grid size: " << gridSize
            << std::endl;
  return true;
}

bool FDTDSolver::reinitialize(int newGridSize) {
  std::cout << "Reinitializing FDTD Solver from " << gridSize << " to " << newGridSize << std::endl;
  
  // Clean up existing resources
  cleanup();
  
  // Reset all texture/program IDs to 0
  texEx = texEy = texEz = texHx = texHy = texHz = 0;
  texEpsilon = texMu = texEmission = 0;
  updateEProgram = updateHProgram = markGeometryProgram = 0;
  triangleSSBO = 0;
  
  // Initialize with new grid size
  return initialize(newGridSize);
}

void FDTDSolver::addEmissionSource(int x, int y, int z, float strength) {
  if (x < 0 || x >= gridSize || y < 0 || y >= gridSize || z < 0 ||
      z >= gridSize) {
    return;
  }

  glBindTexture(GL_TEXTURE_3D, texEmission);
  glTexSubImage3D(GL_TEXTURE_3D, 0, x, y, z, 1, 1, 1, GL_RED, GL_FLOAT,
                  &strength);
}

void FDTDSolver::clearEmission() {
  std::vector<float> zeros(gridSize * gridSize * gridSize, 0.0f);
  glBindTexture(GL_TEXTURE_3D, texEmission);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, zeros.data());
}

void FDTDSolver::update() {
  int workGroups = (gridSize + 7) / 8;

  // Update E field
  glUseProgram(updateEProgram);
  glBindImageTexture(0, texEx, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
  glBindImageTexture(1, texEy, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
  glBindImageTexture(2, texEz, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
  glBindImageTexture(3, texHx, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32F);
  glBindImageTexture(4, texHy, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32F);
  glBindImageTexture(5, texHz, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32F);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, texEpsilon);
  glUniform1i(glGetUniformLocation(updateEProgram, "epsilon"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_3D, texMu);
  glUniform1i(glGetUniformLocation(updateEProgram, "mu"), 1);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_3D, texEmission);
  glUniform1i(glGetUniformLocation(updateEProgram, "emission"), 2);

  glUniform1i(glGetUniformLocation(updateEProgram, "gridSize"), gridSize);
  glDispatchCompute(workGroups, workGroups, workGroups);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  // Update H field
  glUseProgram(updateHProgram);
  glBindImageTexture(0, texEx, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32F);
  glBindImageTexture(1, texEy, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32F);
  glBindImageTexture(2, texEz, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32F);
  glBindImageTexture(3, texHx, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
  glBindImageTexture(4, texHy, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
  glBindImageTexture(5, texHz, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_3D, texEpsilon);
  glUniform1i(glGetUniformLocation(updateHProgram, "epsilon"), 0);

  glUniform1i(glGetUniformLocation(updateHProgram, "gridSize"), gridSize);
  glDispatchCompute(workGroups, workGroups, workGroups);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void FDTDSolver::reset() {
  // Reset all field textures to zero
  std::vector<float> zeros(gridSize * gridSize * gridSize, 0.0f);

  glBindTexture(GL_TEXTURE_3D, texEx);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, zeros.data());

  glBindTexture(GL_TEXTURE_3D, texEy);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, zeros.data());

  glBindTexture(GL_TEXTURE_3D, texEz);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, zeros.data());

  glBindTexture(GL_TEXTURE_3D, texHx);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, zeros.data());

  glBindTexture(GL_TEXTURE_3D, texHy);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, zeros.data());

  glBindTexture(GL_TEXTURE_3D, texHz);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, gridSize, gridSize, gridSize,
                  GL_RED, GL_FLOAT, zeros.data());

  clearEmission();
}

void FDTDSolver::markGeometryGPU(const glm::vec3 &gridCenter,
                                 const glm::vec3 &gridHalfSize,
                                 const SpatialIndex &spatialIndex,
                                 float groundLevel, float materialEpsilon) {
  if (!markGeometryProgram) {
    std::cerr << "Mark geometry program not loaded!" << std::endl;
    return;
  }

  // Get triangles from spatial index
  const auto &triangles = spatialIndex.getTriangles();

  // Prepare triangle data for GPU (aligned struct)
  struct GPUTriangle {
    glm::vec3 v0;
    float pad0;
    glm::vec3 v1;
    float pad1;
    glm::vec3 v2;
    float pad2;
  };

  std::vector<GPUTriangle> gpuTriangles;
  gpuTriangles.reserve(triangles.size());

  // Only include triangles within a reasonable distance of the grid (with per-axis padding)
  glm::vec3 maxDist = gridHalfSize * 1.5f; // 50% padding per axis
  glm::vec3 gridMin = gridCenter - maxDist;
  glm::vec3 gridMax = gridCenter + maxDist;

  for (const auto &tri : triangles) {
    // Simple bounding check - if any vertex is near the grid, include it
    bool nearGrid = false;
    for (const auto &v : {tri.v0, tri.v1, tri.v2}) {
      if (v.x >= gridMin.x && v.x <= gridMax.x && v.y >= gridMin.y &&
          v.y <= gridMax.y && v.z >= gridMin.z && v.z <= gridMax.z) {
        nearGrid = true;
        break;
      }
    }

    if (nearGrid) {
      GPUTriangle gpuTri;
      gpuTri.v0 = tri.v0;
      gpuTri.v1 = tri.v1;
      gpuTri.v2 = tri.v2;
      gpuTri.pad0 = gpuTri.pad1 = gpuTri.pad2 = 0.0f;
      gpuTriangles.push_back(gpuTri);
    }
  }

  std::cout << "Uploading " << gpuTriangles.size()
            << " triangles to GPU (filtered from " << triangles.size()
            << " total)..." << std::endl;

  // Create/update SSBO with triangle data
  if (triangleSSBO == 0) {
    glGenBuffers(1, &triangleSSBO);
  }

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               gpuTriangles.size() * sizeof(GPUTriangle), gpuTriangles.data(),
               GL_STATIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangleSSBO);

  glUseProgram(markGeometryProgram);

  // Bind epsilon texture as writable image
  glBindImageTexture(0, texEpsilon, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);

  // Set uniforms
  glUniform3f(glGetUniformLocation(markGeometryProgram, "gridCenter"),
              gridCenter.x, gridCenter.y, gridCenter.z);
  glUniform3f(glGetUniformLocation(markGeometryProgram, "gridHalfSize"),
              gridHalfSize.x, gridHalfSize.y, gridHalfSize.z);
  glUniform1i(glGetUniformLocation(markGeometryProgram, "gridSize"), gridSize);
  glUniform1f(glGetUniformLocation(markGeometryProgram, "materialEpsilon"),
              materialEpsilon);
  glUniform1f(glGetUniformLocation(markGeometryProgram, "groundLevel"),
              groundLevel);
  glUniform1i(glGetUniformLocation(markGeometryProgram, "numTriangles"),
              static_cast<int>(gpuTriangles.size()));

  // Dispatch compute shader
  int workGroups = (gridSize + 7) / 8;
  glDispatchCompute(workGroups, workGroups, workGroups);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  std::cout << "Geometry marking complete (GPU compute shader)" << std::endl;
}

void FDTDSolver::cleanup() {
  if (texEx)
    glDeleteTextures(1, &texEx);
  if (texEy)
    glDeleteTextures(1, &texEy);
  if (texEz)
    glDeleteTextures(1, &texEz);
  if (texHx)
    glDeleteTextures(1, &texHx);
  if (texHy)
    glDeleteTextures(1, &texHy);
  if (texHz)
    glDeleteTextures(1, &texHz);
  if (texEpsilon)
    glDeleteTextures(1, &texEpsilon);
  if (texMu)
    glDeleteTextures(1, &texMu);
  if (texEmission)
    glDeleteTextures(1, &texEmission);
  if (updateEProgram)
    glDeleteProgram(updateEProgram);
  if (updateHProgram)
    glDeleteProgram(updateHProgram);
  if (markGeometryProgram)
    glDeleteProgram(markGeometryProgram);

  if (triangleSSBO)
    glDeleteBuffers(1, &triangleSSBO);
}
