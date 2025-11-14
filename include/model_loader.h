#pragma once
#include <string>
#include <vector>

struct ModelData {
  std::vector<float> vertices;
  std::vector<unsigned int> indices;
  bool loaded = false;
};

class ModelLoader {
public:
  static ModelData loadOBJ(const std::string &filepath);
};