#pragma once
#include "radio_system.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

class Camera;
class SpatialIndex;

class NodeManager {
public:
  NodeManager(RadioSystem &radioSystem);
  ~NodeManager();

  // Node creation and deletion
  int createNode(const glm::vec3 &position, float frequency = 2.4e9f,
                 NodeType type = NodeType::TRANSMITTER);
  void deleteNode(int id);
  void deleteSelectedNode();
  void clearAllNodes();

  // Node selection
  void selectNode(int id);
  void deselectAll();
  int getSelectedNodeId() const { return selectedNodeId; }
  RadioSource *getSelectedNode();

  // Node manipulation
  void moveSelectedNode(const glm::vec3 &newPosition);
  void setNodePosition(int id, const glm::vec3 &position);

  // Ray picking for node selection and placement
  int pickNode(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
               float maxDistance = 10000.0f);
  glm::vec3 pickPosition(const glm::vec3 &rayOrigin,
                         const glm::vec3 &rayDirection,
                         const SpatialIndex *spatialIndex, bool &hit);

  // Screen to world ray conversion
  static void screenToWorldRay(int mouseX, int mouseY, int screenWidth,
                               int screenHeight, const Camera &camera,
                               glm::vec3 &rayOrigin, glm::vec3 &rayDirection);

  // Placement mode
  void setPlacementMode(bool enabled) { placementMode = enabled; }
  bool isPlacementMode() const { return placementMode; }
  void setPlacementType(NodeType type) { placementNodeType = type; }
  NodeType getPlacementType() const { return placementNodeType; }

  // Get all nodes
  const std::vector<RadioSource> &getNodes() const {
    return radioSystem.getSources();
  }

  RadioSystem *getRadioSystem() { return &radioSystem; }
  const RadioSystem *getRadioSystem() const { return &radioSystem; }

  // Update (for animations, etc.)
  void update(float deltaTime);

private:
  RadioSystem &radioSystem;
  int selectedNodeId;
  bool placementMode;
  NodeType placementNodeType;

  // Helper function to check ray-sphere intersection for node picking
  bool raySphereIntersect(const glm::vec3 &rayOrigin,
                          const glm::vec3 &rayDirection,
                          const glm::vec3 &sphereCenter, float sphereRadius,
                          float &distance);
};
