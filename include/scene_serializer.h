#pragma once

#include <glm/glm.hpp>
#include <string>

// Forward declarations
class NodeManager;
class FDTDSolver;
class Camera;
class VolumeRenderer;

struct SceneData {
  // Camera settings
  glm::vec3 cameraPosition;
  float cameraYaw;
  float cameraPitch;

  // Grid settings
  glm::vec3 fdtdGridHalfSize;
  float voxelSpacing;
  float conductivity;

  // Visualization settings
  glm::vec3 gradientColorLow;
  glm::vec3 gradientColorHigh;
  bool showEmissionSource;
  bool showGeometryEdges;
};

class SceneSerializer {
public:
  // Save scene to file
  static bool saveScene(const std::string &filepath,
                        const NodeManager *nodeManager,
                        const SceneData &sceneData);

  // Load scene from file
  static bool loadScene(const std::string &filepath, NodeManager *nodeManager,
                        SceneData &sceneData);

private:
  // Helper functions for parsing
  static std::string serializeVec3(const glm::vec3 &v);
  static glm::vec3 parseVec3(const std::string &str);
  static std::string trim(const std::string &str);
};
