#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

enum class NodeType { TRANSMITTER, RECEIVER, RELAY };

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

  RadioSource(int nodeId, const glm::vec3 &pos, float freq, float pwr,
              NodeType nodeType = NodeType::TRANSMITTER)
      : id(nodeId), name("Node_" + std::to_string(nodeId)), type(nodeType),
        active(true), position(pos), orientation(0.0f, 0.0f, 0.0f),
        frequency(freq), power(pwr), antennaGain(0.0f), antennaHeight(0.0f),
        selected(false), visible(true) {
    color = frequencyToColor(freq);
  }

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
