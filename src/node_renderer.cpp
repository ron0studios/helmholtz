#include "node_renderer.h"
#include "radio_system.h"
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

NodeRenderer::NodeRenderer()
    : nodeShaderProgram(0), sphereVAO(0), sphereVBO(0), sphereEBO(0),
      sphereIndexCount(0) {}

NodeRenderer::~NodeRenderer() { cleanup(); }

bool NodeRenderer::initialize() {
  nodeShaderProgram = createShaderProgram(nodeVertexShader, nodeFragmentShader);
  if (nodeShaderProgram == 0) {
    std::cerr << "Failed to create node shader program" << std::endl;
    return false;
  }

  createSphere(8.0f, 16);

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
  if (nodeShaderProgram != 0) {
    glDeleteProgram(nodeShaderProgram);
    nodeShaderProgram = 0;
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
