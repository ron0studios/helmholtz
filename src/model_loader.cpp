#include "model_loader.h"

#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <sstream>
#include <unordered_map>

ModelData ModelLoader::loadOBJ(const std::string &filepath) {
  ModelData data;
  std::ifstream file(filepath);

  if (!file.is_open()) {
    std::cerr << "Failed to open OBJ file: " << filepath << std::endl;
    return {};
  }

  std::cout << "Loading OBJ model: " << filepath << std::endl;

  std::vector<glm::vec3> temp_vertices;
  std::vector<glm::vec3> temp_normals;
  std::vector<unsigned int> vertex_indices, normal_indices;

  std::unordered_map<std::string, unsigned int> vertex_map;
  std::string line;
  int line_count = 0;

  while (std::getline(file, line)) {
    line_count++;

    if (line_count % 10000 == 0) {
      std::cout << "Processing line " << line_count << "..." << std::endl;
    }

    std::istringstream iss(line);
    std::string prefix;
    iss >> prefix;

    if (prefix == "v") {

      float x, y, z;
      iss >> x >> y >> z;
      temp_vertices.push_back(glm::vec3(x, y, z));
    } else if (prefix == "vn") {

      float nx, ny, nz;
      iss >> nx >> ny >> nz;
      temp_normals.push_back(glm::vec3(nx, ny, nz));
    } else if (prefix == "f") {
      std::string vertex1, vertex2, vertex3;
      iss >> vertex1 >> vertex2 >> vertex3;

      auto parse_face_vertex =
          [](const std::string &vertex_str) -> std::pair<int, int> {
        size_t first_slash = vertex_str.find('/');
        int v_idx = std::stoi(vertex_str.substr(0, first_slash)) - 1;

        int n_idx = -1;
        if (first_slash != std::string::npos) {
          size_t second_slash = vertex_str.find('/', first_slash + 1);
          if (second_slash != std::string::npos &&
              second_slash + 1 < vertex_str.length()) {
            n_idx = std::stoi(vertex_str.substr(second_slash + 1)) - 1;
          }
        }
        return {v_idx, n_idx};
      };

      auto [v1, n1] = parse_face_vertex(vertex1);
      auto [v2, n2] = parse_face_vertex(vertex2);
      auto [v3, n3] = parse_face_vertex(vertex3);

      vertex_indices.push_back(v1);
      vertex_indices.push_back(v2);
      vertex_indices.push_back(v3);

      normal_indices.push_back(n1);
      normal_indices.push_back(n2);
      normal_indices.push_back(n3);
    }
  }

  file.close();

  std::cout << "Loaded " << temp_vertices.size() << " vertices, "
            << vertex_indices.size() / 3 << " triangles" << std::endl;

  bool has_normals = !temp_normals.empty() && normal_indices[0] >= 0;

  if (!has_normals) {
    std::cout << "No normals found, generating flat normals..." << std::endl;
  }

  for (size_t i = 0; i < vertex_indices.size(); i += 3) {

    glm::vec3 v0 = temp_vertices[vertex_indices[i]];
    glm::vec3 v1 = temp_vertices[vertex_indices[i + 1]];
    glm::vec3 v2 = temp_vertices[vertex_indices[i + 2]];

    glm::vec3 normal;
    if (has_normals) {

      glm::vec3 n0 = (normal_indices[i] >= 0) ? temp_normals[normal_indices[i]]
                                              : glm::vec3(0, 1, 0);
      glm::vec3 n1 = (normal_indices[i + 1] >= 0)
                         ? temp_normals[normal_indices[i + 1]]
                         : glm::vec3(0, 1, 0);
      glm::vec3 n2 = (normal_indices[i + 2] >= 0)
                         ? temp_normals[normal_indices[i + 2]]
                         : glm::vec3(0, 1, 0);
      normal = glm::normalize(n0 + n1 + n2);
    } else {

      glm::vec3 edge1 = v1 - v0;
      glm::vec3 edge2 = v2 - v0;
      normal = glm::normalize(glm::cross(edge1, edge2));
    }

    for (int j = 0; j < 3; j++) {
      glm::vec3 vertex = (j == 0) ? v0 : (j == 1) ? v1 : v2;

      data.vertices.push_back(vertex.x);
      data.vertices.push_back(vertex.y);
      data.vertices.push_back(vertex.z);

      data.vertices.push_back(normal.x);
      data.vertices.push_back(normal.y);
      data.vertices.push_back(normal.z);

      data.indices.push_back(static_cast<unsigned int>(data.indices.size()));
    }
  }

  data.loaded = true;
  std::cout << "OBJ model loaded successfully!" << std::endl;
  std::cout << "Final vertex count: " << data.vertices.size() / 6 << std::endl;
  std::cout << "Triangle count: " << data.indices.size() / 3 << std::endl;

  return data;
}
