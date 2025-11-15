#include "node_manager.h"
#include "camera.h"
#include "spatial_index.h"
#include <algorithm>
#include <limits>

NodeManager::NodeManager(RadioSystem &rs)
    : radioSystem(rs), selectedNodeId(-1), placementMode(false),
      placementNodeType(NodeType::TRANSMITTER) {}

NodeManager::~NodeManager() {}

int NodeManager::createNode(const glm::vec3 &position, float frequency,
                            float power, NodeType type) {
  int id = radioSystem.addSource(position, frequency, power, type);
  return id;
}

void NodeManager::deleteNode(int id) {
  if (selectedNodeId == id) {
    selectedNodeId = -1;
  }
  radioSystem.removeSource(id);
}

void NodeManager::deleteSelectedNode() {
  if (selectedNodeId >= 0) {
    deleteNode(selectedNodeId);
  }
}

void NodeManager::selectNode(int id) {
  if (selectedNodeId >= 0) {
    RadioSource *prevNode = radioSystem.getSourceById(selectedNodeId);
    if (prevNode) {
      prevNode->selected = false;
    }
  }

  selectedNodeId = id;
  RadioSource *node = radioSystem.getSourceById(id);
  if (node) {
    node->selected = true;
  }
}

void NodeManager::deselectAll() {
  if (selectedNodeId >= 0) {
    RadioSource *node = radioSystem.getSourceById(selectedNodeId);
    if (node) {
      node->selected = false;
    }
    selectedNodeId = -1;
  }
}

RadioSource *NodeManager::getSelectedNode() {
  if (selectedNodeId < 0)
    return nullptr;
  return radioSystem.getSourceById(selectedNodeId);
}

void NodeManager::moveSelectedNode(const glm::vec3 &newPosition) {
  if (selectedNodeId >= 0) {
    setNodePosition(selectedNodeId, newPosition);
  }
}

void NodeManager::setNodePosition(int id, const glm::vec3 &position) {
  RadioSource *node = radioSystem.getSourceById(id);
  if (node) {
    node->position = position;
  }
}

int NodeManager::pickNode(const glm::vec3 &rayOrigin,
                          const glm::vec3 &rayDirection, float maxDistance) {
  const float pickRadius = 10.0f;
  float closestDistance = maxDistance;
  int closestNodeId = -1;

  const auto &nodes = radioSystem.getSources();
  for (const auto &node : nodes) {
    if (!node.visible)
      continue;

    float distance;
    if (raySphereIntersect(rayOrigin, rayDirection, node.position, pickRadius,
                           distance)) {
      if (distance < closestDistance) {
        closestDistance = distance;
        closestNodeId = node.id;
      }
    }
  }

  return closestNodeId;
}

glm::vec3 NodeManager::pickPosition(const glm::vec3 &rayOrigin,
                                    const glm::vec3 &rayDirection,
                                    const SpatialIndex *spatialIndex,
                                    bool &hit) {
  hit = false;

  if (spatialIndex) {
    Ray ray;
    ray.origin = rayOrigin;
    ray.direction = rayDirection;
    ray.tMin = 0.1f;
    ray.tMax = 10000.0f;

    RayHit rayHit = spatialIndex->intersect(ray);
    if (rayHit.hit) {
      hit = true;
      return rayHit.point + rayHit.normal * 5.0f;
    }
  }

  return rayOrigin + rayDirection * 500.0f;
}

void NodeManager::screenToWorldRay(int mouseX, int mouseY, int screenWidth,
                                   int screenHeight, const Camera &camera,
                                   glm::vec3 &rayOrigin,
                                   glm::vec3 &rayDirection) {
  float x = (2.0f * mouseX) / screenWidth - 1.0f;
  float y = 1.0f - (2.0f * mouseY) / screenHeight;

  glm::vec4 rayClip(x, y, -1.0f, 1.0f);

  glm::mat4 projMatrix = camera.getProjectionMatrix();
  glm::vec4 rayEye = glm::inverse(projMatrix) * rayClip;
  rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

  glm::mat4 viewMatrix = camera.getViewMatrix();
  glm::vec4 rayWorld = glm::inverse(viewMatrix) * rayEye;
  rayDirection = glm::normalize(glm::vec3(rayWorld));

  rayOrigin = camera.getPosition();
}

void NodeManager::update(float deltaTime) {}

bool NodeManager::raySphereIntersect(const glm::vec3 &rayOrigin,
                                     const glm::vec3 &rayDirection,
                                     const glm::vec3 &sphereCenter,
                                     float sphereRadius, float &distance) {
  glm::vec3 oc = rayOrigin - sphereCenter;
  float a = glm::dot(rayDirection, rayDirection);
  float b = 2.0f * glm::dot(oc, rayDirection);
  float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
  float discriminant = b * b - 4.0f * a * c;

  if (discriminant < 0.0f) {
    return false;
  }

  float t = (-b - sqrt(discriminant)) / (2.0f * a);
  if (t < 0.0f) {
    t = (-b + sqrt(discriminant)) / (2.0f * a);
  }

  if (t < 0.0f) {
    return false;
  }

  distance = t;
  return true;
}
