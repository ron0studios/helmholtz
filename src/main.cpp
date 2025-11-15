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
};

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  windowWidth = width;
  windowHeight = height;
  glViewport(0, 0, width, height);
  camera.setAspectRatio((float)width / (float)height);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  camera.processMouseScroll(static_cast<float>(yoffset));
}

void mouse_button_callback(GLFWwindow *window, int button, int action,
                           int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
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
    NodeManager::screenToWorldRay((int)xpos, (int)ypos, windowWidth,
                                  windowHeight, camera, rayOrigin,
                                  rayDirection);

    if (appState->nodeManager->isPlacementMode()) {
      bool hit;
      glm::vec3 position = appState->nodeManager->pickPosition(
          rayOrigin, rayDirection, appState->spatialIndex, hit);

      NodeType type = appState->nodeManager->getPlacementType();
      appState->nodeManager->createNode(position, 2.4e9f, 20.0f, type);

      appState->showPlacementPreview = false;
    } else {
      int nodeId = appState->nodeManager->pickNode(rayOrigin, rayDirection);
      if (nodeId >= 0) {
        appState->nodeManager->selectNode(nodeId);
      } else {
        appState->nodeManager->deselectAll();
      }
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
  if (!nodeRenderer.initialize()) {
    std::cerr << "Failed to initialize node renderer" << std::endl;
    return -1;
  }

  nodeManager.createNode(glm::vec3(100.0f, 150.0f, 100.0f), 2.4e9f, 20.0f,
                         NodeType::TRANSMITTER);
  nodeManager.createNode(glm::vec3(-100.0f, 120.0f, -100.0f), 2.4e9f, 20.0f,
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
  float fdtdGridHalfSize = 200.0f; // Grid spans 400 units in world space

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
        fdtdGridCenter = (minPos + maxPos) * 0.5f;
        
        // Size the grid to encompass all transmitters with some padding
        glm::vec3 size = maxPos - minPos;
        float maxDim = glm::max(glm::max(size.x, size.y), size.z);
        fdtdGridHalfSize = glm::max(maxDim * 0.75f, 100.0f); // At least 100 units
      }
    }

    // Update FDTD simulation if enabled
    if (appState.fdtdEnabled && !appState.fdtdPaused) {
      for (int i = 0; i < appState.fdtdSimulationSpeed; i++) {
        // Add continuous oscillating source if enabled
        if (appState.fdtdContinuousEmission) {
          fdtdSolver.clearEmission();
          appState.fdtdEmissionPhase += 0.1f;
          float oscillation = std::sin(appState.fdtdEmissionPhase) *
                              appState.fdtdEmissionStrength;

          // Place emission sources at all active transmitter node positions
          const auto &nodes = nodeManager.getNodes();
          for (const auto &node : nodes) {
            if (node.type == NodeType::TRANSMITTER && node.active) {
              // Convert world position to grid coordinates
              glm::vec3 localPos = node.position - fdtdGridCenter;
              glm::vec3 gridPos = (localPos / fdtdGridHalfSize) * 0.5f + 0.5f;
              
              // Convert to integer grid indices
              int sx = static_cast<int>(gridPos.x * FDTD_GRID_SIZE);
              int sy = static_cast<int>(gridPos.y * FDTD_GRID_SIZE);
              int sz = static_cast<int>(gridPos.z * FDTD_GRID_SIZE);
              
              // Clamp to grid bounds
              sx = glm::clamp(sx, 0, FDTD_GRID_SIZE - 1);
              sy = glm::clamp(sy, 0, FDTD_GRID_SIZE - 1);
              sz = glm::clamp(sz, 0, FDTD_GRID_SIZE - 1);

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

    nodeRenderer.render(radioSystem, view, projection);

    // Render FDTD volume if enabled
    if (appState.fdtdEnabled) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(
          GL_FALSE); // Don't write to depth buffer for transparent volume

      volumeRenderer.render(fdtdSolver.getEzTexture(),
                            fdtdSolver.getEpsilonTexture(),
                            fdtdSolver.getEmissionTexture(), view, projection,
                            fdtdGridCenter, fdtdGridHalfSize, FDTD_GRID_SIZE);

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

    uiManager.beginFrame();
    uiManager.render(camera, fps, deltaTime, &nodeManager);
    uiManager.renderFDTDPanel(
        appState.fdtdEnabled, appState.fdtdPaused, appState.fdtdSimulationSpeed,
        appState.fdtdEmissionStrength, appState.fdtdContinuousEmission,
        fdtdGridCenter, fdtdGridHalfSize, appState.fdtdAutoCenterGrid,
        &fdtdSolver, &volumeRenderer);
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