#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>

class Camera;
class NodeManager;

class UIManager {
public:
  UIManager();
  ~UIManager();

  // Initialize ImGui context and backends
  bool initialize(GLFWwindow *window,
                  const char *glsl_version = "#version 330");

  // Begin new ImGui frame
  void beginFrame();

  // Render all UI windows
  void render(const Camera &camera, float fps, float deltaTime,
              NodeManager *nodeManager = nullptr);

  // Render FDTD controls (pass app state from main.cpp)
  void renderFDTDPanel(bool &fdtdEnabled, bool &fdtdPaused,
                       int &simulationSpeed, float &emissionStrength,
                       bool &continuousEmission, glm::vec3 &gridCenter,
                       glm::vec3 &gridHalfSize, bool &autoCenterGrid,
                       void *fdtdSolverPtr, void *volumeRendererPtr);

  // End frame and render ImGui
  void endFrame();

  // Cleanup ImGui resources
  void cleanup();

  // Check if mouse is over any ImGui window
  bool wantCaptureMouse() const;

  // Check if keyboard is captured by ImGui
  bool wantCaptureKeyboard() const;

  // Set whether mouse look mode is active (disables ImGui input capture)
  void setMouseLookMode(bool enabled);

  // Set scene data reference for save/load
  void setSceneDataPointers(void *sceneDataPtr);

  // Check if scene was just loaded
  bool wasSceneLoaded() const { return sceneJustLoaded; }
  void clearSceneLoadedFlag() { sceneJustLoaded = false; }

  // Render FDTD control panel
  void renderFDTDPanel(bool &fdtdEnabled, bool &fdtdPaused,
                       int &simulationSpeed, float &emissionStrength,
                       bool &continuousEmission, glm::vec3 &gridCenter,
                       float &gridHalfSize, bool &autoCenterGrid,
                       void *fdtdSolverPtr, void *volumeRendererPtr);

  // UI state
  struct UIState {
    bool showAboutWindow = false;
    bool showDemoWindow = false;
    bool showPerformanceWindow = true;
    bool showNodePanel = true;
    bool showFDTDPanel = true;
  } state;

private:
  void renderAboutWindow();
  void renderPerformanceWindow(float fps, float deltaTime);
  void renderNodePanel(NodeManager *nodeManager, const Camera &camera);

  bool initialized = false;
  GLFWwindow *window = nullptr;

  // Scene data for save/load
  void *sceneDataPtr = nullptr;
  bool sceneJustLoaded = false;

  // Performance tracking
  static const int FPS_SAMPLE_COUNT = 60;
  float fpsHistory[FPS_SAMPLE_COUNT] = {0};
  int fpsHistoryIndex = 0;
};
