#include "fdtd_solver.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

FDTDSolver::FDTDSolver()
    : gridSize(0), texEx(0), texEy(0), texEz(0), texHx(0), texHy(0), texHz(0),
      texEpsilon(0), texMu(0), texEmission(0), updateEProgram(0),
      updateHProgram(0) {}

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

  if (updateEProgram == 0 || updateHProgram == 0) {
    std::cerr << "Failed to create FDTD compute shaders" << std::endl;
    return false;
  }

  std::cout << "FDTD Solver initialized with grid size: " << gridSize
            << std::endl;
  return true;
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
}
