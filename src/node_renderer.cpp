#include "node_renderer.h"
#include "radio_system.h"
#include "fdtd_solver.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

const std::string NodeRenderer::nodeVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const std::string NodeRenderer::nodeFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 nodeColor;
uniform bool isSelected;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * nodeColor;
    
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * nodeColor;
    
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * vec3(1.0);
    
    vec3 result = ambient + diffuse + specular;
    
    if (isSelected) {
        result = mix(result, vec3(1.0, 1.0, 0.0), 0.4);
    }
    
    FragColor = vec4(result, 1.0);
}
)";

const std::string NodeRenderer::gridVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 nodePosition;

void main() {
    vec3 worldPos = nodePosition + aPos;
    gl_Position = projection * view * vec4(worldPos, 1.0);
    gl_PointSize = 2.0;
}
)";

const std::string NodeRenderer::gridFragmentShader = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 gridColor;

void main() {
    FragColor = vec4(gridColor, 0.6);
}
)";

const std::string NodeRenderer::volumeVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 worldPos;
out vec3 texCoord;

void main() {
    worldPos = vec3(model * vec4(aPos, 1.0));
    texCoord = aPos * 0.5 + 0.5;
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
)";

const std::string NodeRenderer::volumeFragmentShader = R"(
#version 330 core
out vec4 FragColor;

in vec3 worldPos;
in vec3 texCoord;

uniform sampler3D volumeData;
uniform vec3 nodePosition;
uniform float gridSize;
uniform float spacing;
uniform vec3 cameraPos;
uniform float colorIntensity;
uniform float opacity;
uniform float brightness;

vec3 worldToGrid(vec3 world) {
    vec3 offset = world - nodePosition;
    vec3 gridCoord = offset / spacing;
    vec3 normalized = (gridCoord + gridSize * 0.5) / gridSize;
    return clamp(normalized, 0.0, 1.0);
}

vec4 sampleVolume(vec3 pos) {
    vec3 gridPos = worldToGrid(pos);
    float fieldValue = texture(volumeData, gridPos).r;
    
    float intensity = fieldValue * colorIntensity;
    float normalized = clamp(intensity, 0.0, 1.0);
    
    float r = min(1.0, normalized * 2.0);
    float g = min(1.0, normalized);
    float b = max(0.0, 1.0 - normalized * 2.0);
    
    vec3 color = vec3(r, g, b) * brightness;
    color = clamp(color, 0.0, 1.0);
    
    float alpha = min(1.0, intensity * opacity);
    
    return vec4(color, alpha);
}

void main() {
    vec3 rayDir = normalize(worldPos - cameraPos);
    vec3 rayPos = worldPos;
    
    float stepSize = spacing * 0.5;
    int maxSteps = 100;
    
    vec4 accumulatedColor = vec4(0.0);
    float accumulatedAlpha = 0.0;
    
    for (int i = 0; i < maxSteps; i++) {
        if (accumulatedAlpha >= 0.95) break;
        
        vec4 sampleColor = sampleVolume(rayPos);
        
        float weight = (1.0 - accumulatedAlpha) * sampleColor.a;
        accumulatedColor.rgb += sampleColor.rgb * weight;
        accumulatedAlpha += weight;
        
        rayPos += rayDir * stepSize;
    }
    
    accumulatedColor.a = accumulatedAlpha;
    
    if (accumulatedColor.a < 0.01) discard;
    
    FragColor = accumulatedColor;
}
)";

NodeRenderer::NodeRenderer()
    : nodeShaderProgram(0), sphereVAO(0), sphereVBO(0), sphereEBO(0),
      sphereIndexCount(0), gridShaderProgram(0), gridVAO(0), gridVBO(0),
      volumeShaderProgram(0), volumeVAO(0), volumeVBO(0), volumeTexture(0) {}

NodeRenderer::~NodeRenderer() { cleanup(); }

bool NodeRenderer::initialize() {
  nodeShaderProgram = createShaderProgram(nodeVertexShader, nodeFragmentShader);
  if (nodeShaderProgram == 0) {
    std::cerr << "Failed to create node shader program" << std::endl;
    return false;
  }

  gridShaderProgram = createShaderProgram(gridVertexShader, gridFragmentShader);
  if (gridShaderProgram == 0) {
    std::cerr << "Failed to create grid shader program" << std::endl;
    return false;
  }

  volumeShaderProgram =
      createShaderProgram(volumeVertexShader, volumeFragmentShader);
  if (volumeShaderProgram == 0) {
    std::cerr << "Failed to create volume shader program" << std::endl;
    return false;
  }

  createSphere(8.0f, 16);
  createGridBuffers();
  createVolumeBuffers();

  glGenTextures(1, &volumeTexture);

  std::cout << "NodeRenderer initialized" << std::endl;
  return true;
}

void NodeRenderer::createSphere(float radius, int segments) {
  std::vector<float> vertices;
  std::vector<unsigned int> indices;

  for (int lat = 0; lat <= segments; lat++) {
    float theta = lat * M_PI / segments;
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);

    for (int lon = 0; lon <= segments; lon++) {
      float phi = lon * 2 * M_PI / segments;
      float sinPhi = sin(phi);
      float cosPhi = cos(phi);

      float x = cosPhi * sinTheta;
      float y = cosTheta;
      float z = sinPhi * sinTheta;

      vertices.push_back(radius * x);
      vertices.push_back(radius * y);
      vertices.push_back(radius * z);

      vertices.push_back(x);
      vertices.push_back(y);
      vertices.push_back(z);
    }
  }

  for (int lat = 0; lat < segments; lat++) {
    for (int lon = 0; lon < segments; lon++) {
      int first = (lat * (segments + 1)) + lon;
      int second = first + segments + 1;

      indices.push_back(first);
      indices.push_back(second);
      indices.push_back(first + 1);

      indices.push_back(second);
      indices.push_back(second + 1);
      indices.push_back(first + 1);
    }
  }

  sphereIndexCount = indices.size();

  glGenVertexArrays(1, &sphereVAO);
  glGenBuffers(1, &sphereVBO);
  glGenBuffers(1, &sphereEBO);

  glBindVertexArray(sphereVAO);

  glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);
}

void NodeRenderer::createGridBuffers() {
  glGenVertexArrays(1, &gridVAO);
  glGenBuffers(1, &gridVBO);

  glBindVertexArray(gridVAO);
  glBindBuffer(GL_ARRAY_BUFFER, gridVBO);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
}

void NodeRenderer::createVolumeBuffers() {
  float cubeVertices[] = {
      -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f,
      0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f, -0.5f,

      -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,
      0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,

      -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f, -0.5f,
      -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,

      0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f,
      0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,

      -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,
      0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f,

      -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,
      0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f};

  glGenVertexArrays(1, &volumeVAO);
  glGenBuffers(1, &volumeVBO);

  glBindVertexArray(volumeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, volumeVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
}

void NodeRenderer::render(const RadioSystem &radioSystem, const glm::mat4 &view,
                          const glm::mat4 &projection) {
  glUseProgram(nodeShaderProgram);

  GLint projLoc = glGetUniformLocation(nodeShaderProgram, "projection");
  GLint viewLoc = glGetUniformLocation(nodeShaderProgram, "view");
  glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);

  glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
  GLint lightPosLoc = glGetUniformLocation(nodeShaderProgram, "lightPos");
  GLint viewPosLoc = glGetUniformLocation(nodeShaderProgram, "viewPos");
  glUniform3fv(lightPosLoc, 1, &cameraPos[0]);
  glUniform3fv(viewPosLoc, 1, &cameraPos[0]);

  const auto &sources = radioSystem.getSources();
  for (const auto &node : sources) {
    if (!node.visible)
      continue;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, node.position);

    float scale = node.selected ? 1.2f : 1.0f;
    model = glm::scale(model, glm::vec3(scale));

    GLint modelLoc = glGetUniformLocation(nodeShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(nodeShaderProgram, "nodeColor");
    GLint selectedLoc = glGetUniformLocation(nodeShaderProgram, "isSelected");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
    glUniform3fv(colorLoc, 1, &node.color[0]);
    glUniform1i(selectedLoc, node.selected ? 1 : 0);

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
  }

  glBindVertexArray(0);
  glUseProgram(0);
}

void NodeRenderer::renderPlacementPreview(const glm::vec3 &position,
                                          const glm::vec3 &color,
                                          const glm::mat4 &view,
                                          const glm::mat4 &projection) {
  glUseProgram(nodeShaderProgram);

  GLint projLoc = glGetUniformLocation(nodeShaderProgram, "projection");
  GLint viewLoc = glGetUniformLocation(nodeShaderProgram, "view");
  glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);

  glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
  GLint lightPosLoc = glGetUniformLocation(nodeShaderProgram, "lightPos");
  GLint viewPosLoc = glGetUniformLocation(nodeShaderProgram, "viewPos");
  glUniform3fv(lightPosLoc, 1, &cameraPos[0]);
  glUniform3fv(viewPosLoc, 1, &cameraPos[0]);

  float time = glfwGetTime();
  float pulseScale = 1.0f + 0.1f * sin(time * 3.0f);

  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, position);
  model = glm::scale(model, glm::vec3(pulseScale));

  GLint modelLoc = glGetUniformLocation(nodeShaderProgram, "model");
  GLint colorLoc = glGetUniformLocation(nodeShaderProgram, "nodeColor");
  GLint selectedLoc = glGetUniformLocation(nodeShaderProgram, "isSelected");

  glm::vec3 previewColor = color * 0.7f + glm::vec3(0.3f);
  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
  glUniform3fv(colorLoc, 1, &previewColor[0]);
  glUniform1i(selectedLoc, 0);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindVertexArray(sphereVAO);
  glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);

  glDisable(GL_BLEND);
  glBindVertexArray(0);
  glUseProgram(0);
}

void NodeRenderer::renderGrids(const RadioSystem &radioSystem,
                                const glm::mat4 &view,
                                const glm::mat4 &projection) {
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(gridShaderProgram);

  GLint projLoc = glGetUniformLocation(gridShaderProgram, "projection");
  GLint viewLoc = glGetUniformLocation(gridShaderProgram, "view");
  glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);

  const auto &sources = radioSystem.getSources();
  for (const auto &node : sources) {
    if (!node.visible || node.useFDTD)
      continue;

    GLint nodePosLoc = glGetUniformLocation(gridShaderProgram, "nodePosition");
    GLint colorLoc = glGetUniformLocation(gridShaderProgram, "gridColor");

    glUniform3fv(nodePosLoc, 1, &node.position[0]);
    glUniform3fv(colorLoc, 1, &node.color[0]);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 node.sampleGrid.points.size() * sizeof(glm::vec3),
                 node.sampleGrid.points.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_POINTS, 0, node.sampleGrid.points.size());
  }

  glBindVertexArray(0);
  glUseProgram(0);
  glDisable(GL_BLEND);
  glDisable(GL_PROGRAM_POINT_SIZE);
}

void NodeRenderer::renderFDTDGrids(const RadioSystem &radioSystem,
                                    const glm::mat4 &view,
                                    const glm::mat4 &projection) {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  glUseProgram(volumeShaderProgram);

  glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);

  const auto &sources = radioSystem.getSources();
  for (const auto &node : sources) {
    if (!node.visible || !node.useFDTD || !node.fdtdSolver)
      continue;

    int gridSize = node.fdtdSolver->getGridSize();
    float spacing = node.fdtdSolver->getSpacing();
    float colorIntensity = node.fdtdSolver->getColorIntensity();
    float opacity = node.fdtdSolver->getOpacity();
    float brightness = node.fdtdSolver->getBrightness();

    std::vector<float> textureData;
    textureData.reserve(gridSize * gridSize * gridSize);

    for (int z = 0; z < gridSize; z++) {
      for (int y = 0; y < gridSize; y++) {
        for (int x = 0; x < gridSize; x++) {
          double mag = node.fdtdSolver->getFieldMagnitude(x, y, z);
          textureData.push_back(static_cast<float>(mag));
        }
      }
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, volumeTexture);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, gridSize, gridSize, gridSize, 0,
                 GL_RED, GL_FLOAT, textureData.data());

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    float volumeSize = gridSize * spacing;
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, node.position);
    model = glm::scale(model, glm::vec3(volumeSize));

    GLint modelLoc = glGetUniformLocation(volumeShaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(volumeShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(volumeShaderProgram, "projection");
    GLint volumeDataLoc =
        glGetUniformLocation(volumeShaderProgram, "volumeData");
    GLint nodePosLoc =
        glGetUniformLocation(volumeShaderProgram, "nodePosition");
    GLint gridSizeLoc = glGetUniformLocation(volumeShaderProgram, "gridSize");
    GLint spacingLoc = glGetUniformLocation(volumeShaderProgram, "spacing");
    GLint cameraPosLoc =
        glGetUniformLocation(volumeShaderProgram, "cameraPos");
    GLint colorIntensityLoc =
        glGetUniformLocation(volumeShaderProgram, "colorIntensity");
    GLint opacityLoc = glGetUniformLocation(volumeShaderProgram, "opacity");
    GLint brightnessLoc =
        glGetUniformLocation(volumeShaderProgram, "brightness");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);
    glUniform1i(volumeDataLoc, 0);
    glUniform3fv(nodePosLoc, 1, &node.position[0]);
    glUniform1f(gridSizeLoc, static_cast<float>(gridSize));
    glUniform1f(spacingLoc, spacing);
    glUniform3fv(cameraPosLoc, 1, &cameraPos[0]);
    glUniform1f(colorIntensityLoc, colorIntensity);
    glUniform1f(opacityLoc, opacity);
    glUniform1f(brightnessLoc, brightness);

    glBindVertexArray(volumeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_3D, 0);
  glUseProgram(0);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void NodeRenderer::cleanup() {
  if (sphereVAO != 0) {
    glDeleteVertexArrays(1, &sphereVAO);
    sphereVAO = 0;
  }
  if (sphereVBO != 0) {
    glDeleteBuffers(1, &sphereVBO);
    sphereVBO = 0;
  }
  if (sphereEBO != 0) {
    glDeleteBuffers(1, &sphereEBO);
    sphereEBO = 0;
  }
  if (gridVAO != 0) {
    glDeleteVertexArrays(1, &gridVAO);
    gridVAO = 0;
  }
  if (gridVBO != 0) {
    glDeleteBuffers(1, &gridVBO);
    gridVBO = 0;
  }
  if (volumeVAO != 0) {
    glDeleteVertexArrays(1, &volumeVAO);
    volumeVAO = 0;
  }
  if (volumeVBO != 0) {
    glDeleteBuffers(1, &volumeVBO);
    volumeVBO = 0;
  }
  if (volumeTexture != 0) {
    glDeleteTextures(1, &volumeTexture);
    volumeTexture = 0;
  }
  if (nodeShaderProgram != 0) {
    glDeleteProgram(nodeShaderProgram);
    nodeShaderProgram = 0;
  }
  if (gridShaderProgram != 0) {
    glDeleteProgram(gridShaderProgram);
    gridShaderProgram = 0;
  }
  if (volumeShaderProgram != 0) {
    glDeleteProgram(volumeShaderProgram);
    volumeShaderProgram = 0;
  }
}

GLuint NodeRenderer::compileShader(const std::string &source, GLenum type) {
  GLuint shader = glCreateShader(type);
  const char *src = source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    std::cerr << "Shader compilation failed:\n" << infoLog << std::endl;
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint NodeRenderer::createShaderProgram(const std::string &vertexSource,
                                         const std::string &fragmentSource) {
  GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
  GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

  if (vertexShader == 0 || fragmentShader == 0) {
    if (vertexShader != 0)
      glDeleteShader(vertexShader);
    if (fragmentShader != 0)
      glDeleteShader(fragmentShader);
    return 0;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(program, 512, nullptr, infoLog);
    std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
    glDeleteProgram(program);
    program = 0;
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}
