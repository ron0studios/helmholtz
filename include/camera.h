#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
public:
  Camera(float fov = 45.0f, float aspect = 16.0f / 9.0f, float near = 0.1f,
         float far = 10000.0f);

  void processInput(GLFWwindow *window, float deltaTime);
  void processMouseMovement(float xoffset, float yoffset,
                            bool constrainPitch = true);
  void processMouseScroll(float yoffset);

  glm::mat4 getViewMatrix() const;
  glm::mat4 getProjectionMatrix() const;

  glm::vec3 getPosition() const { return position; }
  glm::vec3 getFront() const { return front; }
  glm::vec3 getUp() const { return up; }
  float getFov() const { return fov; }

  void setAspectRatio(float aspect) {
    aspectRatio = aspect;
    updateProjectionMatrix();
  }

private:
  glm::vec3 position;
  glm::vec3 front;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 worldUp;

  float yaw;
  float pitch;

  float movementSpeed;
  float mouseSensitivity;
  float zoom;

  float fov;
  float aspectRatio;
  float nearPlane;
  float farPlane;

  glm::mat4 projectionMatrix;

  void updateCameraVectors();
  void updateProjectionMatrix();
};