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
              NodeManager *nodeManager = nullptr,
              class RadioSystem *radioSystem = nullptr,
              bool *needsToRunSimulation = nullptr);

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

  // UI state
  struct UIState {
    bool showControlPanel = true;
    bool showAboutWindow = false;
    bool showDemoWindow = false;
    bool showPerformanceWindow = true;
    bool showNodePanel = true;
    bool showPropagationPanel = true;
  } state;

private:
  void renderControlPanel(const Camera &camera, float fps, float deltaTime);
  void renderAboutWindow();
  void renderPerformanceWindow(float fps, float deltaTime);
  void renderNodePanel(NodeManager *nodeManager);
  void renderPropagationPanel(class RadioSystem *radioSystem,
                              bool *needsToRunSimulation);

  bool initialized = false;
  GLFWwindow *window = nullptr;

  // Performance tracking
  static const int FPS_SAMPLE_COUNT = 60;
  float fpsHistory[FPS_SAMPLE_COUNT] = {0};
  int fpsHistoryIndex = 0;
};
