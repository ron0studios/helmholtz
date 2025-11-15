#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

class RadioSystem;
class Camera;

enum class GizmoAxis {
  NONE = 0,
  X = 1,
  Y = 2,
  Z = 3
};

class NodeRenderer {
public:
  NodeRenderer();
  ~NodeRenderer();

  bool initialize();
  void render(const RadioSystem &radioSystem, const glm::mat4 &view,
              const glm::mat4 &projection, int selectedNodeId = -1);
  void renderPlacementPreview(const glm::vec3 &position, const glm::vec3 &color,
                              const glm::mat4 &view,
                              const glm::mat4 &projection);
  
  // Transform gizmo rendering
  void renderGizmo(const glm::vec3 &position, const glm::mat4 &view,
                   const glm::mat4 &projection, const Camera &camera);
  
  // Gizmo interaction
  GizmoAxis pickGizmo(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                      const glm::vec3 &gizmoPosition, const Camera &camera);
  
  void cleanup();

private:
  // Sphere rendering for nodes
  GLuint nodeShaderProgram;
  GLuint sphereVAO, sphereVBO, sphereEBO;
  size_t sphereIndexCount;
  
  // Gizmo rendering
  GLuint gizmoShaderProgram;
  GLuint gizmoVAO, gizmoVBO;
  GizmoAxis highlightedAxis = GizmoAxis::NONE;

  void createSphere(float radius, int segments);
  void createGizmo();
  bool rayIntersectCylinder(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                           const glm::vec3 &cylinderStart, const glm::vec3 &cylinderEnd,
                           float radius, float &t);
  
  GLuint compileShader(const std::string &source, GLenum type);
  GLuint createShaderProgram(const std::string &vertexSource,
                             const std::string &fragmentSource);

  static const std::string nodeVertexShader;
  static const std::string nodeFragmentShader;
  static const std::string gizmoVertexShader;
  static const std::string gizmoFragmentShader;
};
