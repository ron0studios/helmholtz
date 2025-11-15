#include "ui_manager.h"
#include "camera.h"
#include "node_manager.h"
#include "fdtd_solver.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>

UIManager::UIManager() {}

UIManager::~UIManager() {
  if (initialized) {
    cleanup();
  }
}

bool UIManager::initialize(GLFWwindow *win, const char *glsl_version) {
  window = win;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  ImGui::StyleColorsDark();

  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 5.0f;
  style.FrameRounding = 3.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;

  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.35f, 0.42f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.45f, 0.55f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.30f, 0.55f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.33f, 0.40f, 0.80f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.33f, 0.40f, 1.00f);

  if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
    std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
    return false;
  }

  if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
    std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
    ImGui_ImplGlfw_Shutdown();
    return false;
  }

  initialized = true;
  std::cout << "ImGui initialized successfully" << std::endl;
  return true;
}

void UIManager::beginFrame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void UIManager::render(const Camera &camera, float fps, float deltaTime,
                       NodeManager *nodeManager) {
  if (state.showControlPanel) {
    renderControlPanel(camera, fps, deltaTime);
  }

  if (state.showPerformanceWindow) {
    renderPerformanceWindow(fps, deltaTime);
  }

  if (state.showNodePanel && nodeManager) {
    renderNodePanel(nodeManager);
  }

  if (state.showAboutWindow) {
    renderAboutWindow();
  }

  if (state.showDemoWindow) {
    ImGui::ShowDemoWindow(&state.showDemoWindow);
  }
}

void UIManager::endFrame() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UIManager::cleanup() {
  if (initialized) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialized = false;
  }
}

bool UIManager::wantCaptureMouse() const {
  return ImGui::GetIO().WantCaptureMouse;
}

bool UIManager::wantCaptureKeyboard() const {
  return ImGui::GetIO().WantCaptureKeyboard;
}

void UIManager::setMouseLookMode(bool enabled) {
  ImGuiIO &io = ImGui::GetIO();
  if (enabled) {
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    io.WantCaptureMouse = false;
    io.WantCaptureKeyboard = false;
  } else {
    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
  }
}

void UIManager::renderControlPanel(const Camera &camera, float fps,
                                   float deltaTime) {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

  ImGui::Begin("Control Panel", &state.showControlPanel);

  if (ImGui::CollapsingHeader("Application", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("RF Propagation Modelling Tool");
    ImGui::Separator();

    if (ImGui::Button("Show Demo Window")) {
      state.showDemoWindow = !state.showDemoWindow;
    }
    ImGui::SameLine();
    if (ImGui::Button("About")) {
      state.showAboutWindow = !state.showAboutWindow;
    }
  }

  if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    glm::vec3 pos = camera.getPosition();
    glm::vec3 front = camera.getFront();
    glm::vec3 up = camera.getUp();

    ImGui::Text("Position:");
    ImGui::BulletText("X: %.2f", pos.x);
    ImGui::BulletText("Y: %.2f", pos.y);
    ImGui::BulletText("Z: %.2f", pos.z);

    ImGui::Spacing();
    ImGui::Text("Direction:");
    ImGui::BulletText("X: %.2f", front.x);
    ImGui::BulletText("Y: %.2f", front.y);
    ImGui::BulletText("Z: %.2f", front.z);

    ImGui::Spacing();
    ImGui::Text("FOV: %.1f°", camera.getFov());
  }

  if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextWrapped("ESC - Exit application");
    ImGui::TextWrapped("TAB - Toggle mouse look");
    ImGui::TextWrapped("WASD - Move camera");
    ImGui::TextWrapped("Q/E - Move up/down");
    ImGui::TextWrapped("SHIFT - Speed boost");
    ImGui::TextWrapped("Mouse - Look around");
    ImGui::TextWrapped("Scroll - Zoom");
  }

  ImGui::End();
}

void UIManager::renderPerformanceWindow(float fps, float deltaTime) {
  ImGui::SetNextWindowPos(ImVec2(10, 420), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(350, 150), ImGuiCond_FirstUseEver);

  ImGui::Begin("Performance", &state.showPerformanceWindow);

  fpsHistory[fpsHistoryIndex] = fps;
  fpsHistoryIndex = (fpsHistoryIndex + 1) % FPS_SAMPLE_COUNT;

  float avgFps = 0.0f;
  for (int i = 0; i < FPS_SAMPLE_COUNT; i++) {
    avgFps += fpsHistory[i];
  }
  avgFps /= FPS_SAMPLE_COUNT;

  ImGui::Text("FPS: %.1f (avg: %.1f)", fps, avgFps);
  ImGui::Text("Frame Time: %.3f ms", deltaTime * 1000.0f);

  ImGui::Spacing();
  ImGui::PlotLines("FPS", fpsHistory, FPS_SAMPLE_COUNT, fpsHistoryIndex, NULL,
                   0.0f, 120.0f, ImVec2(0, 80));

  ImGui::End();
}

void UIManager::renderAboutWindow() {
  ImGui::SetNextWindowPos(ImVec2(400, 100), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);

  ImGui::Begin("About", &state.showAboutWindow);

  ImGui::TextWrapped("RF Propagation Modelling Tool");
  ImGui::Separator();

  ImGui::TextWrapped("Real-time 3D RF propagation simulator with interactive "
                     "node placement and GPU-accelerated computation.");

  ImGui::Spacing();
  ImGui::Text("Version: 0.1.0 (Phase 1 - ImGui Integration)");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Technologies:");
  ImGui::BulletText("OpenGL 3.3+");
  ImGui::BulletText("Dear ImGui %s", IMGUI_VERSION);
  ImGui::BulletText("GLFW");
  ImGui::BulletText("GLEW");
  ImGui::BulletText("GLM");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextWrapped("Project: helmholtz");
  ImGui::TextWrapped("Built for Junction25");

  if (ImGui::Button("Close")) {
    state.showAboutWindow = false;
  }

  ImGui::End();
}

void UIManager::renderNodePanel(NodeManager *nodeManager) {
  ImGui::SetNextWindowPos(ImVec2(10, 580), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

  ImGui::Begin("Nodes", &state.showNodePanel);

  if (ImGui::CollapsingHeader("Placement", ImGuiTreeNodeFlags_DefaultOpen)) {
    bool placementMode = nodeManager->isPlacementMode();
    if (ImGui::Checkbox("Placement Mode", &placementMode)) {
      nodeManager->setPlacementMode(placementMode);
    }

    if (placementMode) {
      ImGui::TextWrapped("Click in 3D view to place node");

      static int nodeTypeIndex = 0;
      const char *nodeTypes[] = {"Transmitter", "Receiver", "Relay"};
      if (ImGui::Combo("Type", &nodeTypeIndex, nodeTypes, 3)) {
        nodeManager->setPlacementType(static_cast<NodeType>(nodeTypeIndex));
      }
    }
  }

  if (ImGui::CollapsingHeader("Node List", ImGuiTreeNodeFlags_DefaultOpen)) {
    const auto &nodes = nodeManager->getNodes();

    ImGui::Text("Total Nodes: %d", (int)nodes.size());
    ImGui::Separator();

    ImGui::BeginChild("NodeListScroll", ImVec2(0, 150), true);

    for (size_t i = 0; i < nodes.size(); i++) {
      const auto &node = nodes[i];

      ImGui::PushID(node.id);

      bool isSelected = node.selected;
      if (ImGui::Selectable(node.name.c_str(), isSelected)) {
        nodeManager->selectNode(node.id);
      }

      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
          nodeManager->deleteNode(node.id);
        }
        ImGui::EndPopup();
      }

      ImGui::PopID();
    }

    ImGui::EndChild();

    if (ImGui::Button("Add Transmitter")) {
      glm::vec3 pos(0.0f, 100.0f, 0.0f);
      nodeManager->createNode(pos, 2.4e9f, 20.0f, NodeType::TRANSMITTER);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected")) {
      nodeManager->deleteSelectedNode();
    }
  }

  RadioSource *selectedNode = nodeManager->getSelectedNode();
  if (selectedNode &&
      ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushItemWidth(200);

    char nameBuf[128];
    strncpy(nameBuf, selectedNode->name.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
      selectedNode->name = nameBuf;
    }

    int typeIndex = static_cast<int>(selectedNode->type);
    const char *nodeTypes[] = {"Transmitter", "Receiver", "Relay"};
    if (ImGui::Combo("Type", &typeIndex, nodeTypes, 3)) {
      selectedNode->type = static_cast<NodeType>(typeIndex);
    }

    ImGui::DragFloat3("Position", &selectedNode->position.x, 1.0f);

    float freqMHz = selectedNode->frequency / 1e6f;
    if (ImGui::DragFloat("Frequency (MHz)", &freqMHz, 0.1f, 1.0f, 10000.0f)) {
      selectedNode->frequency = freqMHz * 1e6f;
      selectedNode->color =
          RadioSource::frequencyToColor(selectedNode->frequency);
    }

    ImGui::DragFloat("Power (dBm)", &selectedNode->power, 0.1f, -100.0f,
                     100.0f);
    ImGui::DragFloat("Antenna Gain (dBi)", &selectedNode->antennaGain, 0.1f,
                     -20.0f, 30.0f);
    ImGui::DragFloat("Antenna Height (m)", &selectedNode->antennaHeight, 0.1f,
                     0.0f, 100.0f);

    ImGui::Checkbox("Active", &selectedNode->active);
    ImGui::Checkbox("Visible", &selectedNode->visible);

    ImGui::ColorEdit3("Color", &selectedNode->color.x);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Sample Grid");

    int gridSize = selectedNode->sampleGrid.gridSize;
    if (ImGui::SliderInt("Grid Size", &gridSize, 1, 100)) {
      selectedNode->sampleGrid.setGridSize(gridSize);
      if (selectedNode->fdtdSolver) {
        selectedNode->fdtdSolver->setGridSize(gridSize);
      }
    }
    ImGui::Text("Total Points: %d", gridSize * gridSize * gridSize);

    float spacing = selectedNode->sampleGrid.spacing;
    if (ImGui::DragFloat("Grid Spacing", &spacing, 0.1f, 0.1f, 100.0f)) {
      selectedNode->sampleGrid.setSpacing(spacing);
      if (selectedNode->fdtdSolver) {
        selectedNode->fdtdSolver->setSpacing(spacing);
      }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("FDTD Simulation");

    bool useFDTD = selectedNode->useFDTD;
    if (ImGui::Checkbox("Enable FDTD", &useFDTD)) {
      selectedNode->enableFDTD(useFDTD);
    }

    if (selectedNode->useFDTD && selectedNode->fdtdSolver) {
      if (ImGui::Button("Reset Simulation")) {
        selectedNode->fdtdSolver->reset();
      }

      ImGui::Text("Time Step: %d", selectedNode->fdtdSolver->getTimeStep());

      ImGui::Spacing();
      ImGui::Text("Source Controls");

      bool sourceEnabled = selectedNode->fdtdSolver->isSourceEnabled();
      if (ImGui::Checkbox("Source Active", &sourceEnabled)) {
        selectedNode->fdtdSolver->setSourceEnabled(sourceEnabled);
      }

      float freqGHz = selectedNode->fdtdSolver->getSourceFrequency() / 1e9f;
      if (ImGui::DragFloat("Frequency (GHz)", &freqGHz, 0.1f, 0.1f, 10.0f)) {
        selectedNode->fdtdSolver->setSourceFrequency(freqGHz * 1e9f);
      }

      float amplitude = selectedNode->fdtdSolver->getSourceAmplitude();
      if (ImGui::DragFloat("Amplitude", &amplitude, 0.01f, 0.0f, 10.0f)) {
        selectedNode->fdtdSolver->setSourceAmplitude(amplitude);
      }

      ImGui::Spacing();
      ImGui::Text("Pulse Modulation");

      bool pulseMode = selectedNode->fdtdSolver->isPulseMode();
      if (ImGui::Checkbox("Enable Pulse Mode", &pulseMode)) {
        selectedNode->fdtdSolver->setPulseMode(pulseMode);
      }

      if (pulseMode) {
        float pulseFreq = selectedNode->fdtdSolver->getPulseFrequency();
        if (ImGui::DragFloat("Pulse Frequency (Hz)", &pulseFreq, 0.1f, 0.1f,
                             100.0f)) {
          selectedNode->fdtdSolver->setPulseFrequency(pulseFreq);
        }
      }

      ImGui::Spacing();
      ImGui::Text("Visualization");

      float colorIntensity = selectedNode->fdtdSolver->getColorIntensity();
      if (ImGui::DragFloat("Color Intensity", &colorIntensity, 1.0f, 1.0f,
                           1000.0f)) {
        selectedNode->fdtdSolver->setColorIntensity(colorIntensity);
      }

      float opacity = selectedNode->fdtdSolver->getOpacity();
      if (ImGui::SliderFloat("Opacity", &opacity, 0.0f, 1.0f)) {
        selectedNode->fdtdSolver->setOpacity(opacity);
      }

      float brightness = selectedNode->fdtdSolver->getBrightness();
      if (ImGui::SliderFloat("Brightness", &brightness, 0.0f, 5.0f)) {
        selectedNode->fdtdSolver->setBrightness(brightness);
      }

      ImGui::Text("Opacity controls volume transparency");
      ImGui::Text("Brightness boosts color intensity");
    }

    ImGui::PopItemWidth();
  }

  ImGui::End();
}
