#include "propagation_renderer.h"
#include "radio_system.h"
#include <iostream>

const std::string PropagationRenderer::vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 view;
uniform mat4 projection;

out vec3 fragColor;

void main() {
    gl_Position = projection * view * vec4(aPos, 1.0);
    fragColor = aColor;
}
)";

const std::string PropagationRenderer::fragmentShaderSource = R"(
#version 330 core
in vec3 fragColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(fragColor, 0.6);
}
)";

PropagationRenderer::PropagationRenderer()
    : shaderProgram(0), VAO(0), VBO(0) {}

PropagationRenderer::~PropagationRenderer() { cleanup(); }

GLuint PropagationRenderer::compileShader(const std::string &source,
                                          GLenum type) {
  GLuint shader = glCreateShader(type);
  const char *src = source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    return 0;
  }

  return shader;
}

GLuint PropagationRenderer::createShaderProgram(
    const std::string &vertexSource, const std::string &fragmentSource) {
  GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
  GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

  if (vertexShader == 0 || fragmentShader == 0) {
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
    std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    return 0;
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

bool PropagationRenderer::initialize() {
  shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
  if (shaderProgram == 0) {
    std::cerr << "Failed to create propagation shader program" << std::endl;
    return false;
  }

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  return true;
}

void PropagationRenderer::updateRayBuffers(const std::vector<SignalRay> &rays) {
  vertexData.clear();
  raySegments.clear();

  for (const auto &ray : rays) {
    if (ray.points.size() < 2)
      continue;

    RaySegment segment;
    segment.startIndex = vertexData.size() / 6;
    segment.vertexCount = ray.points.size();

    for (const auto &point : ray.points) {
      vertexData.push_back(point.x);
      vertexData.push_back(point.y);
      vertexData.push_back(point.z);

      vertexData.push_back(ray.color.r);
      vertexData.push_back(ray.color.g);
      vertexData.push_back(ray.color.b);
    }

    raySegments.push_back(segment);
  }

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float),
               vertexData.data(), GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void PropagationRenderer::render(const glm::mat4 &view,
                                 const glm::mat4 &projection) {
  if (raySegments.empty())
    return;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(shaderProgram);

  GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
  GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
  glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);

  glBindVertexArray(VAO);

  glLineWidth(2.0f);

  for (const auto &segment : raySegments) {
    glDrawArrays(GL_LINE_STRIP, segment.startIndex, segment.vertexCount);
  }

  glBindVertexArray(0);
  glDisable(GL_BLEND);
}

void PropagationRenderer::cleanup() {
  if (VAO != 0) {
    glDeleteVertexArrays(1, &VAO);
    VAO = 0;
  }
  if (VBO != 0) {
    glDeleteBuffers(1, &VBO);
    VBO = 0;
  }
  if (shaderProgram != 0) {
    glDeleteProgram(shaderProgram);
    shaderProgram = 0;
  }
}
