#include "scene_serializer.h"
#include "camera.h"
#include "node_manager.h"
#include "radio_system.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

std::string SceneSerializer::serializeVec3(const glm::vec3 &v) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(6);
  oss << v.x << "," << v.y << "," << v.z;
  return oss.str();
}

glm::vec3 SceneSerializer::parseVec3(const std::string &str) {
  glm::vec3 result(0.0f);
  std::istringstream iss(str);
  std::string token;

  if (std::getline(iss, token, ','))
    result.x = std::stof(token);
  if (std::getline(iss, token, ','))
    result.y = std::stof(token);
  if (std::getline(iss, token, ','))
    result.z = std::stof(token);

  return result;
}

std::string SceneSerializer::trim(const std::string &str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, last - first + 1);
}

bool SceneSerializer::saveScene(const std::string &filepath,
                                const NodeManager *nodeManager,
                                const SceneData &sceneData) {
  std::ofstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to open file for writing: " << filepath << std::endl;
    return false;
  }

  file << std::fixed << std::setprecision(6);

  // Write header
  file << "# Helmholtz Scene File\n";
  file << "# Generated scene configuration\n\n";

  // Write camera settings
  file << "[Camera]\n";
  file << "position=" << serializeVec3(sceneData.cameraPosition) << "\n";
  file << "yaw=" << sceneData.cameraYaw << "\n";
  file << "pitch=" << sceneData.cameraPitch << "\n\n";

  // Write grid settings
  file << "[Grid]\n";
  file << "halfSize=" << serializeVec3(sceneData.fdtdGridHalfSize) << "\n";
  file << "voxelSpacing=" << sceneData.voxelSpacing << "\n";
  file << "conductivity=" << sceneData.conductivity << "\n\n";

  // Write visualization settings
  file << "[Visualization]\n";
  file << "gradientColorLow=" << serializeVec3(sceneData.gradientColorLow)
       << "\n";
  file << "gradientColorHigh=" << serializeVec3(sceneData.gradientColorHigh)
       << "\n";
  file << "showEmissionSource="
       << (sceneData.showEmissionSource ? "true" : "false") << "\n";
  file << "showGeometryEdges="
       << (sceneData.showGeometryEdges ? "true" : "false") << "\n\n";

  // Write nodes
  const auto &sources = nodeManager->getRadioSystem()->getSources();
  file << "[Nodes]\n";
  file << "count=" << sources.size() << "\n\n";

  for (size_t i = 0; i < sources.size(); ++i) {
    const RadioSource &node = sources[i];
    file << "[Node" << i << "]\n";
    file << "id=" << node.id << "\n";
    file << "name=" << node.name << "\n";
    file << "type=" << static_cast<int>(node.type) << "\n";
    file << "active=" << (node.active ? "true" : "false") << "\n";
    file << "position=" << serializeVec3(node.position) << "\n";
    file << "orientation=" << serializeVec3(node.orientation) << "\n";
    file << "frequency=" << node.frequency << "\n";
    file << "color=" << serializeVec3(node.color) << "\n";
    file << "visible=" << (node.visible ? "true" : "false") << "\n\n";
  }

  file.close();
  std::cout << "Scene saved to: " << filepath << std::endl;
  return true;
}

bool SceneSerializer::loadScene(const std::string &filepath,
                                NodeManager *nodeManager,
                                SceneData &sceneData) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to open file for reading: " << filepath << std::endl;
    return false;
  }

  std::string line;
  std::string currentSection;
  int nodeCount = 0;

  // Temporary node data
  struct TempNodeData {
    int id;
    std::string name;
    NodeType type;
    bool active;
    glm::vec3 position;
    glm::vec3 orientation;
    float frequency;
    glm::vec3 color;
    bool visible;
  };
  std::vector<TempNodeData> tempNodes;
  TempNodeData currentNode;

  while (std::getline(file, line)) {
    line = trim(line);

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#')
      continue;

    // Check for section headers
    if (line[0] == '[' && line.back() == ']') {
      currentSection = line.substr(1, line.length() - 2);
      if (currentSection.find("Node") == 0) {
        // Start a new node
        currentNode = TempNodeData();
      }
      continue;
    }

    // Parse key=value pairs
    size_t equalPos = line.find('=');
    if (equalPos == std::string::npos)
      continue;

    std::string key = trim(line.substr(0, equalPos));
    std::string value = trim(line.substr(equalPos + 1));

    // Parse based on current section
    if (currentSection == "Camera") {
      if (key == "position")
        sceneData.cameraPosition = parseVec3(value);
      else if (key == "yaw")
        sceneData.cameraYaw = std::stof(value);
      else if (key == "pitch")
        sceneData.cameraPitch = std::stof(value);
    } else if (currentSection == "Grid") {
      if (key == "halfSize")
        sceneData.fdtdGridHalfSize = parseVec3(value);
      else if (key == "voxelSpacing")
        sceneData.voxelSpacing = std::stof(value);
      else if (key == "conductivity")
        sceneData.conductivity = std::stof(value);
    } else if (currentSection == "Visualization") {
      if (key == "gradientColorLow")
        sceneData.gradientColorLow = parseVec3(value);
      else if (key == "gradientColorHigh")
        sceneData.gradientColorHigh = parseVec3(value);
      else if (key == "showEmissionSource")
        sceneData.showEmissionSource = (value == "true");
      else if (key == "showGeometryEdges")
        sceneData.showGeometryEdges = (value == "true");
    } else if (currentSection == "Nodes") {
      if (key == "count")
        nodeCount = std::stoi(value);
    } else if (currentSection.find("Node") == 0) {
      // Parse node data
      if (key == "id")
        currentNode.id = std::stoi(value);
      else if (key == "name")
        currentNode.name = value;
      else if (key == "type")
        currentNode.type = static_cast<NodeType>(std::stoi(value));
      else if (key == "active")
        currentNode.active = (value == "true");
      else if (key == "position")
        currentNode.position = parseVec3(value);
      else if (key == "orientation")
        currentNode.orientation = parseVec3(value);
      else if (key == "frequency")
        currentNode.frequency = std::stof(value);
      else if (key == "color")
        currentNode.color = parseVec3(value);
      else if (key == "visible") {
        currentNode.visible = (value == "true");
        // This is the last field, so save the node
        tempNodes.push_back(currentNode);
      }
    }
  }

  file.close();

  // Clear existing nodes
  nodeManager->clearAllNodes();

  // Create nodes from loaded data
  for (const auto &nodeData : tempNodes) {
    int newId = nodeManager->createNode(nodeData.position, nodeData.frequency,
                                        nodeData.type);
    RadioSource *node = nodeManager->getRadioSystem()->getSourceById(newId);
    if (node) {
      node->name = nodeData.name;
      node->active = nodeData.active;
      node->orientation = nodeData.orientation;
      node->color = nodeData.color;
      node->visible = nodeData.visible;
    }
  }

  std::cout << "Scene loaded from: " << filepath << std::endl;
  std::cout << "Loaded " << tempNodes.size() << " nodes" << std::endl;

  return true;
}
