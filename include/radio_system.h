#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

class FDTDSolver;

enum class NodeType { TRANSMITTER, RECEIVER, RELAY };

struct SampleGrid {
  int gridSize;
  float spacing;
  std::vector<glm::vec3> points;

  SampleGrid() : gridSize(50), spacing(1.0f) { generateGrid(); }

  void generateGrid() {
    points.clear();
    points.reserve(gridSize * gridSize * gridSize);
    int center = gridSize / 2;
    for (int x = 0; x < gridSize; x++) {
      for (int y = 0; y < gridSize; y++) {
        for (int z = 0; z < gridSize; z++) {
          float xPos = (x - center) * spacing;
          float yPos = (y - center) * spacing;
          float zPos = (z - center) * spacing;
          points.push_back(glm::vec3(xPos, yPos, zPos));
        }
      }
    }
  }

  void setGridSize(int size) {
    if (size < 1)
      size = 1;
    if (size > 100)
      size = 100;
    gridSize = size;
    generateGrid();
  }

  void setSpacing(float s) {
    if (s < 0.1f)
      s = 0.1f;
    if (s > 100.0f)
      s = 100.0f;
    spacing = s;
    generateGrid();
  }
};

struct RadioSource {
  int id;
  std::string name;
  NodeType type;
  bool active;

  glm::vec3 position;
  glm::vec3 orientation;

  float frequency;
  float power;
  float antennaGain;
  float antennaHeight;

  glm::vec3 color;
  bool selected;
  bool visible;

  SampleGrid sampleGrid;
  std::unique_ptr<FDTDSolver> fdtdSolver;
  bool useFDTD;

  RadioSource(int nodeId, const glm::vec3 &pos, float freq, float pwr,
              NodeType nodeType = NodeType::TRANSMITTER);

  RadioSource(const RadioSource &other);
  RadioSource(RadioSource &&other) noexcept = default;
  RadioSource &operator=(const RadioSource &other);
  RadioSource &operator=(RadioSource &&other) noexcept = default;

  void enableFDTD(bool enable);
  void updateFDTD();

  static glm::vec3 frequencyToColor(float freq) {
    if (freq < 1e9)
      return glm::vec3(1.0f, 0.3f, 0.3f);
    else if (freq < 2.5e9)
      return glm::vec3(0.3f, 1.0f, 0.3f);
    else
      return glm::vec3(0.3f, 0.3f, 1.0f);
  }

  static const char *nodeTypeToString(NodeType type) {
    switch (type) {
    case NodeType::TRANSMITTER:
      return "Transmitter";
    case NodeType::RECEIVER:
      return "Receiver";
    case NodeType::RELAY:
      return "Relay";
    default:
      return "Unknown";
    }
  }
};

struct SignalRay {
  glm::vec3 origin;
  glm::vec3 direction;
  float strength;
  int bounces;
  glm::vec3 color;
  std::vector<glm::vec3> points;
};

class RadioSystem {
public:
  RadioSystem();
  ~RadioSystem();

  int addSource(const glm::vec3 &position, float frequency, float power,
                NodeType type = NodeType::TRANSMITTER);
  void removeSource(int id);
  void removeSourceByIndex(int index);
  RadioSource *getSourceById(int id);
  RadioSource *getSourceByIndex(int index);
  int getSourceCount() const { return sources.size(); }

  void update(float deltaTime);

  void computeSignalPropagation(const class SpatialIndex *spatialIndex);

  const std::vector<RadioSource> &getSources() const { return sources; }
  std::vector<RadioSource> &getSources() { return sources; }
  const std::vector<SignalRay> &getSignalRays() const { return signalRays; }

  void setRaysPerSource(int count) { raysPerSource = count; }
  void setMaxBounces(int count) { maxBounces = count; }
  void setMaxDistance(float dist) { maxDistance = dist; }

private:
  std::vector<RadioSource> sources;
  std::vector<SignalRay> signalRays;

  int nextNodeId;
  int raysPerSource;
  int maxBounces;
  float maxDistance;

  float calculatePathLoss(float distance, float frequency);
  float calculateReflectionLoss(const glm::vec3 &normal);
};
