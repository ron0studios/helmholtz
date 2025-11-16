#pragma once
#include <glm/glm.hpp>

struct VisualSettings {
  // Fog settings
  bool enableFog = true;
  float fogDensity = 0.0003f;
  glm::vec3 fogColor = glm::vec3(0.5f, 0.7f, 1.0f);
};
