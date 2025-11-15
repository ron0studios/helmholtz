#pragma once
#include <glm/glm.hpp>
#include <vector>

#define IDX(x, y, z) ((x) + gridSize * ((y) + gridSize * (z)))

class FDTDSolver {
public:
  FDTDSolver(int size = 50, float cellSize = 1.0f);
  ~FDTDSolver();

  void reset();
  void step();
  void update(bool sourceActive, int sourceX, int sourceY, int sourceZ);

  void setGridSize(int size);
  void setSpacing(float spacing);
  void setSourcePosition(int x, int y, int z);
  void setSourceFrequency(float freq);
  void setSourceAmplitude(float amp);
  void setColorIntensity(float intensity);
  void setSourceEnabled(bool enabled);
  void setPulseMode(bool enabled);
  void setPulseFrequency(float freq);
  void setOpacity(float opacity);
  void setBrightness(float brightness);

  int getGridSize() const { return gridSize; }
  float getSpacing() const { return cellSpacing; }
  glm::vec3 getSourcePosition() const { return sourcePos; }
  float getSourceFrequency() const { return sourceFrequency; }
  float getSourceAmplitude() const { return sourceAmplitude; }
  float getColorIntensity() const { return colorIntensity; }
  bool isSourceEnabled() const { return sourceEnabled; }
  bool isPulseMode() const { return pulseMode; }
  float getPulseFrequency() const { return pulseFrequency; }
  int getTimeStep() const { return timeStep; }
  float getOpacity() const { return opacity; }
  float getBrightness() const { return brightness; }

  const std::vector<glm::vec3> &getGridPoints() const { return gridPoints; }
  const std::vector<glm::vec3> &getGridColors() const { return gridColors; }

  double getFieldMagnitude(int x, int y, int z) const;
  glm::vec3 getEField(int x, int y, int z) const;
  glm::vec3 getHField(int x, int y, int z) const;

private:
  int gridSize;
  float cellSpacing;
  glm::vec3 sourcePos;

  std::vector<double> Ex, Ey, Ez;
  std::vector<double> Hx, Hy, Hz;

  std::vector<glm::vec3> gridPoints;
  std::vector<glm::vec3> gridColors;

  double DT;
  double DX, DY, DZ;
  double sourceFrequency;
  double sourceAmplitude;
  int timeStep;
  float colorIntensity;
  bool sourceEnabled;
  bool pulseMode;
  float pulseFrequency;
  float opacity;
  float brightness;

  static constexpr double C0 = 299792458.0;
  static constexpr double EPSILON_0 = 8.854187817e-12;
  static constexpr double MU_0 = 1.2566370614e-6;
  static constexpr double DAMPING_FACTOR = 0.999;

  void updateH();
  void updateE(bool isSourceOn, int sx, int sy, int sz);
  void updateGridPoints();
  void updateGridColors();

  bool isValidCell(int x, int y, int z) const {
    return x >= 0 && x < gridSize && y >= 0 && y < gridSize && z >= 0 &&
           z < gridSize;
  }
};
