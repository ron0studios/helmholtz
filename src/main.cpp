#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "camera.h"
#include "model_loader.h"
#include "renderer.h"

Camera camera(45.0f, 1920.0f / 1080.0f, 0.1f, 10000.0f);
float lastX = 960.0f;
float lastY = 540.0f;
bool firstMouse = true;
bool mouseEnabled = false;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

int windowWidth = 1920;
int windowHeight = 1080;

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  windowWidth = width;
  windowHeight = height;
  glViewport(0, 0, width, height);
  camera.setAspectRatio((float)width / (float)height);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  if (!mouseEnabled)
    return;

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

  camera.processMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  camera.processMouseScroll(static_cast<float>(yoffset));
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }

  if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
    mouseEnabled = !mouseEnabled;
    if (mouseEnabled) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      firstMouse = true;
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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
  glfwSetKeyCallback(window, key_callback);

  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
    return -1;
  }

  std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)
            << std::endl;

  printControls();

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

  std::cout << "\\nStarting render loop. Press TAB to enable mouse look."
            << std::endl;

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    camera.processInput(window, deltaTime);

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();
    glm::mat4 model = glm::mat4(1.0f);

    renderer.render(view, projection, model);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  renderer.cleanup();
  glfwTerminate();

  std::cout << "Application closed successfully." << std::endl;
  return 0;
}