#include "camera.h"

#include <algorithm>

Camera::Camera(float fov, float aspect, float near, float far)
    : position(glm::vec3(0.0f, 100.0f, 500.0f)),
      front(glm::vec3(0.0f, 0.0f, -1.0f)), worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
      yaw(-90.0f), pitch(-10.0f), movementSpeed(250.0f), mouseSensitivity(0.1f),
      zoom(45.0f), fov(fov), aspectRatio(aspect), nearPlane(near),
      farPlane(far) {
  updateCameraVectors();
  updateProjectionMatrix();
}

void Camera::processInput(GLFWwindow *window, float deltaTime) {
  float velocity = movementSpeed * deltaTime;

  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    velocity *= 3.0f;
  }

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    position += front * velocity;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    position -= front * velocity;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    position -= right * velocity;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    position += right * velocity;
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    position += worldUp * velocity;
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    position -= worldUp * velocity;
}

void Camera::processMouseMovement(float xoffset, float yoffset,
                                  bool constrainPitch) {
  xoffset *= mouseSensitivity;
  yoffset *= mouseSensitivity;

  yaw += xoffset;
  pitch += yoffset;

  if (constrainPitch) {
    if (pitch > 89.0f)
      pitch = 89.0f;
    if (pitch < -89.0f)
      pitch = -89.0f;
  }

  updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset) {
  zoom -= yoffset;
  if (zoom < 1.0f)
    zoom = 1.0f;
  if (zoom > 45.0f)
    zoom = 45.0f;
}

glm::mat4 Camera::getViewMatrix() const {
  return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix() const { return projectionMatrix; }

void Camera::updateCameraVectors() {
  glm::vec3 newFront;
  newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  newFront.y = sin(glm::radians(pitch));
  newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  front = glm::normalize(newFront);

  right = glm::normalize(glm::cross(front, worldUp));
  up = glm::normalize(glm::cross(right, front));
}

void Camera::updateProjectionMatrix() {
  projectionMatrix =
      glm::perspective(glm::radians(zoom), aspectRatio, nearPlane, farPlane);
}