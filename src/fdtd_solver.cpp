#include "fdtd_solver.h"
#include <algorithm>
#include <cmath>
#include <iostream>

#ifdef _OPENMP
#include <omp.h>
#endif

FDTDSolver::FDTDSolver(int size, float cellSize)
    : gridSize(size), cellSpacing(cellSize), sourcePos(size / 2, size / 2,
                                                       size / 2),
      sourceFrequency(2.4e9), sourceAmplitude(1.0), timeStep(0),
      colorIntensity(100.0f), sourceEnabled(true), pulseMode(false),
      pulseFrequency(1.0f), opacity(0.8f), brightness(1.0f) {
  DX = DY = DZ = cellSpacing;
  DT = DX / (C0 * sqrt(3.0));

  int totalCells = gridSize * gridSize * gridSize;
  Ex.resize(totalCells, 0.0);
  Ey.resize(totalCells, 0.0);
  Ez.resize(totalCells, 0.0);
  Hx.resize(totalCells, 0.0);
  Hy.resize(totalCells, 0.0);
  Hz.resize(totalCells, 0.0);

  updateGridPoints();
  updateGridColors();

  std::cout << "FDTD Solver initialized: " << gridSize << "^3 grid, "
            << "DT=" << DT << "s" << std::endl;
}

FDTDSolver::~FDTDSolver() {}

void FDTDSolver::reset() {
  std::fill(Ex.begin(), Ex.end(), 0.0);
  std::fill(Ey.begin(), Ey.end(), 0.0);
  std::fill(Ez.begin(), Ez.end(), 0.0);
  std::fill(Hx.begin(), Hx.end(), 0.0);
  std::fill(Hy.begin(), Hy.end(), 0.0);
  std::fill(Hz.begin(), Hz.end(), 0.0);
  timeStep = 0;
  updateGridColors();
}

void FDTDSolver::setGridSize(int size) {
  if (size < 1)
    size = 1;
  if (size > 150)
    size = 150;

  gridSize = size;
  sourcePos = glm::vec3(size / 2, size / 2, size / 2);

  DX = DY = DZ = cellSpacing;
  DT = DX / (C0 * sqrt(3.0));

  int totalCells = gridSize * gridSize * gridSize;
  Ex.resize(totalCells);
  Ey.resize(totalCells);
  Ez.resize(totalCells);
  Hx.resize(totalCells);
  Hy.resize(totalCells);
  Hz.resize(totalCells);

  reset();
  updateGridPoints();
}

void FDTDSolver::setSpacing(float spacing) {
  if (spacing < 0.001f)
    spacing = 0.001f;
  if (spacing > 100.0f)
    spacing = 100.0f;

  cellSpacing = spacing;
  DX = DY = DZ = cellSpacing;
  DT = DX / (C0 * sqrt(3.0));

  updateGridPoints();
}

void FDTDSolver::setSourcePosition(int x, int y, int z) {
  if (isValidCell(x, y, z)) {
    sourcePos = glm::vec3(x, y, z);
  }
}

void FDTDSolver::setSourceFrequency(float freq) { sourceFrequency = freq; }

void FDTDSolver::setSourceAmplitude(float amp) { sourceAmplitude = amp; }

void FDTDSolver::setColorIntensity(float intensity) {
  colorIntensity = intensity;
}

void FDTDSolver::setSourceEnabled(bool enabled) { sourceEnabled = enabled; }

void FDTDSolver::setPulseMode(bool enabled) { pulseMode = enabled; }

void FDTDSolver::setPulseFrequency(float freq) { pulseFrequency = freq; }

void FDTDSolver::setOpacity(float op) {
  opacity = std::clamp(op, 0.0f, 1.0f);
}

void FDTDSolver::setBrightness(float bright) {
  brightness = std::clamp(bright, 0.0f, 5.0f);
}

void FDTDSolver::updateGridPoints() {
  gridPoints.clear();
  gridPoints.reserve(gridSize * gridSize * gridSize);

  int center = gridSize / 2;
  for (int x = 0; x < gridSize; x++) {
    for (int y = 0; y < gridSize; y++) {
      for (int z = 0; z < gridSize; z++) {
        float xPos = (x - center) * cellSpacing;
        float yPos = (y - center) * cellSpacing;
        float zPos = (z - center) * cellSpacing;
        gridPoints.push_back(glm::vec3(xPos, yPos, zPos));
      }
    }
  }
}

void FDTDSolver::updateGridColors() {
  int totalPoints = gridSize * gridSize * gridSize;
  gridColors.resize(totalPoints);

#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (int i = 0; i < totalPoints; ++i) {
    double ex = Ex[i];
    double ey = Ey[i];
    double ez = Ez[i];

    double mag = sqrt(ex * ex + ey * ey + ez * ez);
    double normalized = std::min(1.0, mag * colorIntensity);

    float r = std::min(1.0f, (float)normalized * 2.0f);
    float g = std::min(1.0f, (float)normalized);
    float b = std::max(0.0f, 1.0f - (float)normalized * 2.0f);

    gridColors[i] = glm::vec3(r, g, b);
  }
}

void FDTDSolver::step() {
  int sx = (int)sourcePos.x;
  int sy = (int)sourcePos.y;
  int sz = (int)sourcePos.z;
  update(true, sx, sy, sz);
}

void FDTDSolver::update(bool sourceActive, int sourceX, int sourceY,
                        int sourceZ) {
  updateH();
  updateE(sourceActive, sourceX, sourceY, sourceZ);
  updateGridColors();
  timeStep++;
}

void FDTDSolver::updateH() {
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (int z = 0; z < gridSize - 1; z++) {
    for (int y = 0; y < gridSize - 1; y++) {
      for (int x = 0; x < gridSize - 1; x++) {
        Hx[IDX(x, y, z)] +=
            (DT / MU_0) * ((Ey[IDX(x, y, z + 1)] - Ey[IDX(x, y, z)]) / DZ -
                           (Ez[IDX(x, y + 1, z)] - Ez[IDX(x, y, z)]) / DY);

        Hy[IDX(x, y, z)] +=
            (DT / MU_0) * ((Ez[IDX(x + 1, y, z)] - Ez[IDX(x, y, z)]) / DX -
                           (Ex[IDX(x, y, z + 1)] - Ex[IDX(x, y, z)]) / DZ);

        Hz[IDX(x, y, z)] +=
            (DT / MU_0) * ((Ex[IDX(x, y + 1, z)] - Ex[IDX(x, y, z)]) / DY -
                           (Ey[IDX(x + 1, y, z)] - Ey[IDX(x, y, z)]) / DX);

        Hx[IDX(x, y, z)] *= DAMPING_FACTOR;
        Hy[IDX(x, y, z)] *= DAMPING_FACTOR;
        Hz[IDX(x, y, z)] *= DAMPING_FACTOR;
      }
    }
  }
}

void FDTDSolver::updateE(bool isSourceOn, int sx, int sy, int sz) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (int z = 1; z < gridSize; z++) {
    for (int y = 1; y < gridSize; y++) {
      for (int x = 1; x < gridSize; x++) {
        Ex[IDX(x, y, z)] +=
            (DT / EPSILON_0) * ((Hz[IDX(x, y, z)] - Hz[IDX(x, y - 1, z)]) / DY -
                                (Hy[IDX(x, y, z)] - Hy[IDX(x, y, z - 1)]) / DZ);

        Ey[IDX(x, y, z)] +=
            (DT / EPSILON_0) * ((Hx[IDX(x, y, z)] - Hx[IDX(x, y, z - 1)]) / DZ -
                                (Hz[IDX(x, y, z)] - Hz[IDX(x - 1, y, z)]) / DX);

        Ez[IDX(x, y, z)] +=
            (DT / EPSILON_0) * ((Hy[IDX(x, y, z)] - Hy[IDX(x - 1, y, z)]) / DX -
                                (Hx[IDX(x, y, z)] - Hx[IDX(x, y - 1, z)]) / DY);

        Ex[IDX(x, y, z)] *= DAMPING_FACTOR;
        Ey[IDX(x, y, z)] *= DAMPING_FACTOR;
        Ez[IDX(x, y, z)] *= DAMPING_FACTOR;
      }
    }
  }

  if (isSourceOn && sourceEnabled && isValidCell(sx, sy, sz)) {
    double omega = 2.0 * M_PI * sourceFrequency;
    double t = timeStep * DT;

    double envelope = 1.0;
    if (pulseMode) {
      double pulseOmega = 2.0 * M_PI * pulseFrequency;
      envelope = (sin(pulseOmega * t) + 1.0) * 0.5;
    }

    double sourceValue = sourceAmplitude * sin(omega * t) * envelope;
    Ez[IDX(sx, sy, sz)] += sourceValue;
  }
}

double FDTDSolver::getFieldMagnitude(int x, int y, int z) const {
  if (!isValidCell(x, y, z))
    return 0.0;

  int idx = IDX(x, y, z);
  double ex = Ex[idx];
  double ey = Ey[idx];
  double ez = Ez[idx];

  return sqrt(ex * ex + ey * ey + ez * ez);
}

glm::vec3 FDTDSolver::getEField(int x, int y, int z) const {
  if (!isValidCell(x, y, z))
    return glm::vec3(0.0f);

  int idx = IDX(x, y, z);
  return glm::vec3(Ex[idx], Ey[idx], Ez[idx]);
}

glm::vec3 FDTDSolver::getHField(int x, int y, int z) const {
  if (!isValidCell(x, y, z))
    return glm::vec3(0.0f);

  int idx = IDX(x, y, z);
  return glm::vec3(Hx[idx], Hy[idx], Hz[idx]);
}
