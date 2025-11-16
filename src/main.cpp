#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cfloat>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "camera.h"
#include "fdtd_solver.h"
#include "model_loader.h"
#include "node_manager.h"
#include "node_renderer.h"
#include "radio_system.h"
#include "renderer.h"
#include "scene_serializer.h"
#include "spatial_index.h"
#include "ui_manager.h"
#include "volume_renderer.h"


Camera camera(45.0f, 1920.0f / 1080.0f, 0.1f, 10000.0f);
float lastX = 960.0f;
float lastY = 540.0f;
bool firstMouse = true;
bool mouseEnabled = false;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

int windowWidth = 1920;
int windowHeight = 1080;

// Forward declaration for callbacks
NodeRenderer *g_nodeRenderer = nullptr;

struct AppState {
  UIManager *uiManager = nullptr;
  NodeManager *nodeManager = nullptr;
  SpatialIndex *spatialIndex = nullptr;

  bool showPlacementPreview = false;
  glm::vec3 placementPreviewPos = glm::vec3(0.0f);

  // FDTD simulation state
  bool fdtdEnabled = false;
  bool fdtdPaused = false;
  int fdtdSimulationSpeed = 1;
  float fdtdEmissionStrength = 0.5f;
  bool fdtdContinuousEmission = true;
  float fdtdEmissionPhase = 0.0f;
  bool fdtdAutoCenterGrid = true;
  bool fdtdShowDebugVisuals = false; // Show geometry outline and grid

  // Gizmo interaction state
  bool isDraggingGizmo = false;
  GizmoAxis draggedAxis = GizmoAxis::NONE;
  glm::vec3 dragStartNodePos = glm::vec3(0.0f);
  glm::vec3 dragStartHitPoint = glm::vec3(0.0f);
};

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  windowWidth = width;
  windowHeight = height;
  glViewport(0, 0, width, height);
  camera.setAspectRatio((float)width / (float)height);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  if (mouseEnabled) {
    // Camera look mode
    if (firstMouse) {
      lastX = static_cast<float>(xpos);
      lastY = static_cast<float>(ypos);
      firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos);
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    camera.processMouseMovement(xoffset, yoffset);
  } else {
    // Gizmo dragging mode
    AppState *appState =
        static_cast<AppState *>(glfwGetWindowUserPointer(window));
    if (appState && appState->isDraggingGizmo && appState->nodeManager &&
        g_nodeRenderer) {
      glm::vec3 rayOrigin, rayDirection;
      NodeManager::screenToWorldRay((int)xpos, (int)ypos, windowWidth,
                                    windowHeight, camera, rayOrigin,
                                    rayDirection);

      RadioSource *selectedNode = appState->nodeManager->getSelectedNode();
      if (selectedNode) {
        // Determine the axis direction
        glm::vec3 axisDirection;
        switch (appState->draggedAxis) {
        case GizmoAxis::X:
          axisDirection = glm::vec3(1, 0, 0);
          break;
        case GizmoAxis::Y:
          axisDirection = glm::vec3(0, 1, 0);
          break;
        case GizmoAxis::Z:
          axisDirection = glm::vec3(0, 0, 1);
          break;
        default:
          return;
        }

        // Create a plane perpendicular to the camera view that contains the
        // axis
        glm::vec3 cameraForward = camera.getFront();
        glm::vec3 planeNormal = glm::normalize(glm::cross(
            axisDirection, glm::cross(cameraForward, axisDirection)));

        // If plane normal is too small, use a fallback plane
        if (glm::length(planeNormal) < 0.01f) {
          glm::vec3 fallback = glm::vec3(0, 1, 0);
          if (std::abs(glm::dot(axisDirection, fallback)) > 0.9f) {
            fallback = glm::vec3(1, 0, 0);
          }
          planeNormal = glm::normalize(glm::cross(axisDirection, fallback));
        }

        // Intersect ray with plane through the start position
        float denom = glm::dot(rayDirection, planeNormal);
        if (std::abs(denom) > 0.0001f) {
          glm::vec3 p0 = appState->dragStartNodePos;
          float t = glm::dot(p0 - rayOrigin, planeNormal) / denom;

          if (t >= 0) {
            glm::vec3 hitPoint = rayOrigin + rayDirection * t;
            glm::vec3 delta = hitPoint - appState->dragStartHitPoint;

            // Project delta onto the axis
            float movement = glm::dot(delta, axisDirection);
            glm::vec3 newPos =
                appState->dragStartNodePos + axisDirection * movement;

            appState->nodeManager->moveSelectedNode(newPos);
          }
        }
      }
    }
  }
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  camera.processMouseScroll(static_cast<float>(yoffset));
}

void mouse_button_callback(GLFWwindow *window, int button, int action,
                           int mods) {
  AppState *appState =
      static_cast<AppState *>(glfwGetWindowUserPointer(window));
  if (!appState || !appState->nodeManager || !appState->uiManager)
    return;

  if (appState->uiManager->wantCaptureMouse())
    return;

  if (mouseEnabled)
    return;

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  glm::vec3 rayOrigin, rayDirection;
  NodeManager::screenToWorldRay((int)xpos, (int)ypos, windowWidth, windowHeight,
                                camera, rayOrigin, rayDirection);

  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    if (action == GLFW_PRESS) {
      if (appState->nodeManager->isPlacementMode()) {
        bool hit;
        glm::vec3 position = appState->nodeManager->pickPosition(
            rayOrigin, rayDirection, appState->spatialIndex, hit);

        NodeType type = appState->nodeManager->getPlacementType();
        appState->nodeManager->createNode(position, 2.4e9f, type);

        appState->showPlacementPreview = false;
      } else {
        // Check if clicking on gizmo first
        int selectedNodeId = appState->nodeManager->getSelectedNodeId();
        if (selectedNodeId >= 0) {
          RadioSource *selectedNode = appState->nodeManager->getSelectedNode();
          if (selectedNode && g_nodeRenderer) {
            GizmoAxis axis = g_nodeRenderer->pickGizmo(
                rayOrigin, rayDirection, selectedNode->position, camera);
            if (axis != GizmoAxis::NONE) {
              // Start gizmo drag - calculate initial hit point on plane
              appState->isDraggingGizmo = true;
              appState->draggedAxis = axis;
              appState->dragStartNodePos = selectedNode->position;

              // Determine the axis direction
              glm::vec3 axisDirection;
              switch (axis) {
              case GizmoAxis::X:
                axisDirection = glm::vec3(1, 0, 0);
                break;
              case GizmoAxis::Y:
                axisDirection = glm::vec3(0, 1, 0);
                break;
              case GizmoAxis::Z:
                axisDirection = glm::vec3(0, 0, 1);
                break;
              default:
                axisDirection = glm::vec3(1, 0, 0);
              }

              // Create initial plane perpendicular to camera that contains axis
              glm::vec3 cameraForward = camera.getFront();
              glm::vec3 planeNormal = glm::normalize(glm::cross(
                  axisDirection, glm::cross(cameraForward, axisDirection)));

              if (glm::length(planeNormal) < 0.01f) {
                glm::vec3 fallback = glm::vec3(0, 1, 0);
                if (std::abs(glm::dot(axisDirection, fallback)) > 0.9f) {
                  fallback = glm::vec3(1, 0, 0);
                }
                planeNormal =
                    glm::normalize(glm::cross(axisDirection, fallback));
              }

              // Calculate initial hit point
              float denom = glm::dot(rayDirection, planeNormal);
              if (std::abs(denom) > 0.0001f) {
                float t =
                    glm::dot(selectedNode->position - rayOrigin, planeNormal) /
                    denom;
                appState->dragStartHitPoint = rayOrigin + rayDirection * t;
              } else {
                appState->dragStartHitPoint = selectedNode->position;
              }

              return;
            }
          }
        }

        // Otherwise, select node
        int nodeId = appState->nodeManager->pickNode(rayOrigin, rayDirection);
        if (nodeId >= 0) {
          appState->nodeManager->selectNode(nodeId);
        } else {
          appState->nodeManager->deselectAll();
        }
      }
    } else if (action == GLFW_RELEASE) {
      // Stop gizmo drag
      appState->isDraggingGizmo = false;
      appState->draggedAxis = GizmoAxis::NONE;
    }
  }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }

  if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
    mouseEnabled = !mouseEnabled;
    AppState *appState =
        static_cast<AppState *>(glfwGetWindowUserPointer(window));
    if (mouseEnabled) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      firstMouse = true;
      if (appState && appState->uiManager) {
        appState->uiManager->setMouseLookMode(true);
      }
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      if (appState && appState->uiManager) {
        appState->uiManager->setMouseLookMode(false);
      }
    }
  }
}

void printControls() {
  std::cout << "\n=== Radio Wave Visualization - Controls ===" << std::endl;
  std::cout << "ESC     - Exit application" << std::endl;
  std::cout << "TAB     - Toggle mouse look" << std::endl;
  std::cout << "WASD    - Move camera (forward/back/left/right)" << std::endl;
  std::cout << "Q/E     - Move camera up/down" << std::endl;
  std::cout << "SHIFT   - Speed boost" << std::endl;
  std::cout << "Mouse   - Look around (when mouse look enabled)" << std::endl;
  std::cout << "Scroll  - Zoom in/out" << std::endl;
  std::cout << "==========================================\\n" << std::endl;
}

int main() {
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight,
                                        "Radio Wave Visualization - Hong Kong",
                                        nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetKeyCallback(window, key_callback);

  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
    return -1;
  }

  std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)
            << std::endl;

  printControls();

  UIManager uiManager;
  if (!uiManager.initialize(window, "#version 330")) {
    std::cerr << "Failed to initialize UI Manager" << std::endl;
    return -1;
  }

  Renderer renderer;
  if (!renderer.initialize(windowWidth, windowHeight)) {
    std::cerr << "Failed to initialize renderer" << std::endl;
    return -1;
  }

  std::cout << "Loading Hong Kong city model..." << std::endl;
  ModelData modelData = ModelLoader::loadOBJ("hongkong.obj");

  if (!modelData.loaded) {
    std::cerr << "Failed to load model" << std::endl;
    return -1;
  }

  std::cout << "Model loaded successfully!" << std::endl;
  std::cout << "Vertices: " << modelData.vertices.size() / 6 << std::endl;
  std::cout << "Triangles: " << modelData.indices.size() / 3 << std::endl;

  renderer.setModelData(modelData.vertices, modelData.indices);

  std::cout << "Initializing spatial index..." << std::endl;
  SpatialIndex spatialIndex;

  const std::string bvhCacheFile = "hongkong.bvh";
  bool bvhLoaded = spatialIndex.loadBVH(bvhCacheFile);

  if (!bvhLoaded) {
    std::cout << "Building spatial index from scratch..." << std::endl;

    std::vector<Triangle> triangles;
    for (size_t i = 0; i < modelData.indices.size(); i += 3) {
      Triangle tri;
      unsigned int i0 = modelData.indices[i];
      unsigned int i1 = modelData.indices[i + 1];
      unsigned int i2 = modelData.indices[i + 2];

      tri.v0 = glm::vec3(modelData.vertices[i0 * 6 + 0],
                         modelData.vertices[i0 * 6 + 1],
                         modelData.vertices[i0 * 6 + 2]);
      tri.v1 = glm::vec3(modelData.vertices[i1 * 6 + 0],
                         modelData.vertices[i1 * 6 + 1],
                         modelData.vertices[i1 * 6 + 2]);
      tri.v2 = glm::vec3(modelData.vertices[i2 * 6 + 0],
                         modelData.vertices[i2 * 6 + 1],
                         modelData.vertices[i2 * 6 + 2]);

      glm::vec3 edge1 = tri.v1 - tri.v0;
      glm::vec3 edge2 = tri.v2 - tri.v0;
      tri.normal = glm::normalize(glm::cross(edge1, edge2));
      tri.id = i / 3;

      triangles.push_back(tri);
    }

    spatialIndex.build(triangles);

    std::cout << "Saving BVH to cache..." << std::endl;
    spatialIndex.saveBVH(bvhCacheFile);
  }

  std::cout << "Spatial index ready!" << std::endl;

  RadioSystem radioSystem;
  NodeManager nodeManager(radioSystem);

  NodeRenderer nodeRenderer;
  g_nodeRenderer = &nodeRenderer; // Set global pointer for callbacks
  if (!nodeRenderer.initialize()) {
    std::cerr << "Failed to initialize node renderer" << std::endl;
    return -1;
  }

  nodeManager.createNode(glm::vec3(100.0f, 150.0f, 100.0f), 2.4e9f,
                         NodeType::TRANSMITTER);
  nodeManager.createNode(glm::vec3(-100.0f, 120.0f, -100.0f), 2.4e9f,
                         NodeType::RECEIVER);

  // Initialize FDTD system
  const int FDTD_GRID_SIZE = 64; // Start with smaller grid for performance
  FDTDSolver fdtdSolver;
  if (!fdtdSolver.initialize(FDTD_GRID_SIZE)) {
    std::cerr << "Failed to initialize FDTD solver" << std::endl;
    return -1;
  }

  VolumeRenderer volumeRenderer;
  if (!volumeRenderer.initialize()) {
    std::cerr << "Failed to initialize volume renderer" << std::endl;
    return -1;
  }

  // FDTD grid parameters - position the grid in world space
  glm::vec3 fdtdGridCenter = glm::vec3(0.0f, 100.0f, 0.0f);
  glm::vec3 fdtdGridHalfSize =
      glm::vec3(200.0f, 200.0f, 200.0f); // Grid dimensions in world space
  glm::vec3 lastFdtdGridCenter = fdtdGridCenter;
  glm::vec3 lastFdtdGridHalfSize = fdtdGridHalfSize;

  // Mark geometry using GPU (instant, no performance impact)
  std::cout << "Marking geometry in FDTD grid using GPU..." << std::endl;
  fdtdSolver.markGeometryGPU(fdtdGridCenter, fdtdGridHalfSize, spatialIndex,
                             0.0f, 50.0f);

  // Create scene data for save/load functionality
  SceneData sceneData;
  sceneData.cameraPosition = camera.getPosition();
  sceneData.cameraYaw = camera.getYaw();
  sceneData.cameraPitch = camera.getPitch();
  sceneData.fdtdGridHalfSize = fdtdGridHalfSize;
  sceneData.voxelSpacing = fdtdSolver.getVoxelSpacing();
  sceneData.conductivity = fdtdSolver.getConductivity();
  sceneData.gradientColorLow = volumeRenderer.getGradientColorLow();
  sceneData.gradientColorHigh = volumeRenderer.getGradientColorHigh();
  sceneData.showEmissionSource = volumeRenderer.getShowEmissionSource();
  sceneData.showGeometryEdges = volumeRenderer.getShowGeometryEdges();

  // Pass scene data pointer to UI manager
  uiManager.setSceneDataPointers(&sceneData);

  AppState appState;
  appState.uiManager = &uiManager;
  appState.nodeManager = &nodeManager;
  appState.spatialIndex = &spatialIndex;
  glfwSetWindowUserPointer(window, &appState);

  std::cout << "\\nStarting render loop. Press TAB to enable mouse look."
            << std::endl;

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    float fps = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;

    if (mouseEnabled) {
      double xpos, ypos;
      glfwGetCursorPos(window, &xpos, &ypos);

      float xposf = static_cast<float>(xpos);
      float yposf = static_cast<float>(ypos);

      if (firstMouse) {
        lastX = xposf;
        lastY = yposf;
        firstMouse = false;
      }

      float xoffset = xposf - lastX;
      float yoffset = lastY - yposf;

      lastX = xposf;
      lastY = yposf;

      if (xoffset != 0.0f || yoffset != 0.0f) {
        camera.processMouseMovement(xoffset, yoffset);
      }
    }

    camera.processInput(window, deltaTime);

    // Auto-center FDTD grid on transmitter nodes if enabled
    if (appState.fdtdEnabled && appState.fdtdAutoCenterGrid) {
      const auto &nodes = nodeManager.getNodes();
      glm::vec3 minPos(FLT_MAX);
      glm::vec3 maxPos(-FLT_MAX);
      int transmitterCount = 0;

      for (const auto &node : nodes) {
        if (node.type == NodeType::TRANSMITTER && node.active) {
          minPos = glm::min(minPos, node.position);
          maxPos = glm::max(maxPos, node.position);
          transmitterCount++;
        }
      }

      if (transmitterCount > 0) {
        // Center the grid on the bounding box of transmitters
        // (only affects position, not size - user controls size manually)
        fdtdGridCenter = (minPos + maxPos) * 0.5f;
      }
    }

    // Calculate required grid size based on voxel spacing (meters per voxel)
    // This ensures constant resolution regardless of physical grid size
    float voxelSpacing = fdtdSolver.getVoxelSpacing();
    int requiredGridSizeX =
        static_cast<int>(std::ceil(fdtdGridHalfSize.x * 2.0f / voxelSpacing));
    int requiredGridSizeY =
        static_cast<int>(std::ceil(fdtdGridHalfSize.y * 2.0f / voxelSpacing));
    int requiredGridSizeZ =
        static_cast<int>(std::ceil(fdtdGridHalfSize.z * 2.0f / voxelSpacing));

    // Use the maximum dimension for a cubic grid
    int requiredGridSize = glm::max(
        glm::max(requiredGridSizeX, requiredGridSizeY), requiredGridSizeZ);

    // Clamp to reasonable range (performance vs detail tradeoff)
    requiredGridSize = glm::clamp(requiredGridSize, 32, 128);

    // Reinitialize if grid size needs to change
    if (requiredGridSize != fdtdSolver.getGridSize()) {
      std::cout << "Grid size changed from " << fdtdSolver.getGridSize()
                << " to " << requiredGridSize
                << " (voxel spacing: " << voxelSpacing << "m)" << std::endl;
      fdtdSolver.reinitialize(requiredGridSize);

      // Force geometry remarking
      lastFdtdGridCenter = fdtdGridCenter + glm::vec3(1000.0f);
    }

    // Re-mark geometry if grid changed (GPU, instant)
    // Use a larger threshold to avoid resetting too frequently
    if (appState.fdtdEnabled &&
        (glm::distance(fdtdGridCenter, lastFdtdGridCenter) > 20.0f ||
         glm::distance(fdtdGridHalfSize, lastFdtdGridHalfSize) > 20.0f)) {
      std::cout << "Grid moved significantly - resetting FDTD simulation..."
                << std::endl;
      fdtdSolver.reset(); // Clear all fields when grid moves
      fdtdSolver.markGeometryGPU(fdtdGridCenter, fdtdGridHalfSize, spatialIndex,
                                 0.0f, 50.0f);
      lastFdtdGridCenter = fdtdGridCenter;
      lastFdtdGridHalfSize = fdtdGridHalfSize;
    }

    // Update FDTD simulation if enabled
    if (appState.fdtdEnabled && !appState.fdtdPaused) {
      for (int i = 0; i < appState.fdtdSimulationSpeed; i++) {
        // Add continuous oscillating source if enabled
        if (appState.fdtdContinuousEmission) {
          fdtdSolver.clearEmission();

          // Speed of light in m/s
          const float c = 3.0e8f;

          // Time step increment (arbitrary time scale for visualization)
          const float dt = 1e-11f; // 10 picoseconds per simulation step
          appState.fdtdEmissionPhase += 2.0f * M_PI * dt;

          // Place emission sources at all active transmitter node positions
          const auto &nodes = nodeManager.getNodes();
          for (const auto &node : nodes) {
            if (node.type == NodeType::TRANSMITTER && node.active) {
              // Calculate oscillation based on actual frequency
              // omega = 2 * pi * f
              float angularFreq = 2.0f * M_PI * node.frequency;
              float oscillation =
                  std::sin(angularFreq * appState.fdtdEmissionPhase /
                           (2.0f * M_PI)) *
                  appState.fdtdEmissionStrength;

              // Convert world position to grid coordinates
              glm::vec3 localPos = node.position - fdtdGridCenter;
              glm::vec3 gridPos = (localPos / fdtdGridHalfSize) * 0.5f + 0.5f;

              // Convert to integer grid indices (use dynamic grid size)
              int currentGridSize = fdtdSolver.getGridSize();
              int sx = static_cast<int>(gridPos.x * currentGridSize);
              int sy = static_cast<int>(gridPos.y * currentGridSize);
              int sz = static_cast<int>(gridPos.z * currentGridSize);

              // Clamp to grid bounds
              sx = glm::clamp(sx, 0, currentGridSize - 1);
              sy = glm::clamp(sy, 0, currentGridSize - 1);
              sz = glm::clamp(sz, 0, currentGridSize - 1);

              fdtdSolver.addEmissionSource(sx, sy, sz, oscillation);
            }
          }
        }
        fdtdSolver.update();
      }
    }

    if (nodeManager.isPlacementMode() && !mouseEnabled &&
        !uiManager.wantCaptureMouse()) {
      double xpos, ypos;
      glfwGetCursorPos(window, &xpos, &ypos);

      glm::vec3 rayOrigin, rayDirection;
      NodeManager::screenToWorldRay((int)xpos, (int)ypos, windowWidth,
                                    windowHeight, camera, rayOrigin,
                                    rayDirection);

      bool hit;
      glm::vec3 position =
          nodeManager.pickPosition(rayOrigin, rayDirection, &spatialIndex, hit);

      appState.placementPreviewPos = position;
      appState.showPlacementPreview = true;
    } else {
      appState.showPlacementPreview = false;
    }

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();
    glm::mat4 model = glm::mat4(1.0f);

    renderer.render(view, projection, model);

    int selectedNodeId = nodeManager.getSelectedNodeId();
    nodeRenderer.render(radioSystem, view, projection, selectedNodeId);

    // Render gizmo for selected node
    if (selectedNodeId >= 0) {
      RadioSource *selectedNode = nodeManager.getSelectedNode();
      if (selectedNode) {
        nodeRenderer.renderGizmo(selectedNode->position, view, projection,
                                 camera);
      }
    }

    // Render FDTD volume if enabled
    if (appState.fdtdEnabled) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(
          GL_FALSE); // Don't write to depth buffer for transparent volume

      volumeRenderer.render(
          fdtdSolver.getEzTexture(), fdtdSolver.getEpsilonTexture(),
          fdtdSolver.getEmissionTexture(), view, projection, fdtdGridCenter,
          fdtdGridHalfSize, fdtdSolver.getGridSize());

      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
    }

    if (appState.showPlacementPreview) {
      glm::vec3 previewColor;
      NodeType placementType = nodeManager.getPlacementType();
      switch (placementType) {
      case NodeType::TRANSMITTER:
        previewColor = glm::vec3(1.0f, 0.3f, 0.3f);
        break;
      case NodeType::RECEIVER:
        previewColor = glm::vec3(0.3f, 1.0f, 0.3f);
        break;
      case NodeType::RELAY:
        previewColor = glm::vec3(0.3f, 0.3f, 1.0f);
        break;
      }
      nodeRenderer.renderPlacementPreview(appState.placementPreviewPos,
                                          previewColor, view, projection);
    }

    // Update scene data for save/load
    sceneData.cameraPosition = camera.getPosition();
    sceneData.cameraYaw = camera.getYaw();
    sceneData.cameraPitch = camera.getPitch();
    sceneData.fdtdGridHalfSize = fdtdGridHalfSize;
    sceneData.voxelSpacing = fdtdSolver.getVoxelSpacing();
    sceneData.conductivity = fdtdSolver.getConductivity();
    sceneData.gradientColorLow = volumeRenderer.getGradientColorLow();
    sceneData.gradientColorHigh = volumeRenderer.getGradientColorHigh();
    sceneData.showEmissionSource = volumeRenderer.getShowEmissionSource();
    sceneData.showGeometryEdges = volumeRenderer.getShowGeometryEdges();

    uiManager.beginFrame();
    uiManager.render(camera, fps, deltaTime, &nodeManager);
    uiManager.renderFDTDPanel(
        appState.fdtdEnabled, appState.fdtdPaused, appState.fdtdSimulationSpeed,
        appState.fdtdEmissionStrength, appState.fdtdContinuousEmission,
        fdtdGridCenter, fdtdGridHalfSize, appState.fdtdAutoCenterGrid,
        &fdtdSolver, &volumeRenderer);

    // Apply loaded scene data if a scene was just loaded
    if (uiManager.wasSceneLoaded()) {
      camera.setPosition(sceneData.cameraPosition);
      camera.setYaw(sceneData.cameraYaw);
      camera.setPitch(sceneData.cameraPitch);
      fdtdGridHalfSize = sceneData.fdtdGridHalfSize;
      fdtdSolver.setVoxelSpacing(sceneData.voxelSpacing);
      fdtdSolver.setConductivity(sceneData.conductivity);
      volumeRenderer.setGradientColorLow(sceneData.gradientColorLow);
      volumeRenderer.setGradientColorHigh(sceneData.gradientColorHigh);
      volumeRenderer.setShowEmissionSource(sceneData.showEmissionSource);
      volumeRenderer.setShowGeometryEdges(sceneData.showGeometryEdges);
      uiManager.clearSceneLoadedFlag();
    }

    uiManager.endFrame();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  renderer.cleanup();
  nodeRenderer.cleanup();
  fdtdSolver.cleanup();
  volumeRenderer.cleanup();
  uiManager.cleanup();
  glfwTerminate();

  std::cout << "Application closed successfully." << std::endl;
  return 0;
}