#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "camera.h"
#include "model_loader.h"
#include "node_manager.h"
#include "node_renderer.h"
#include "radio_system.h"
#include "renderer.h"
#include "spatial_index.h"
#include "ui_manager.h"

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

    radioSystem.update(deltaTime);

    auto &sources = radioSystem.getSources();
    for (auto &source : sources) {
      if (source.useFDTD && source.active) {
        source.updateFDTD();
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
    nodeRenderer.renderGrids(radioSystem, view, projection);
    nodeRenderer.renderFDTDGrids(radioSystem, view, projection);

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
    uiManager.endFrame();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  renderer.cleanup();
  nodeRenderer.cleanup();
  uiManager.cleanup();
  glfwTerminate();

  std::cout << "Application closed successfully." << std::endl;
  return 0;
}