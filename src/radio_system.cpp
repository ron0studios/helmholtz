#include "radio_system.h"
#include "spatial_index.h"
#include "diffraction_edge.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

RadioSystem::RadioSystem()
    : nextNodeId(1), raysPerSource(64), maxBounces(2), maxDistance(2000.0f),
      enableDiffraction(true) {}

RadioSystem::~RadioSystem() {}

int RadioSystem::addSource(const glm::vec3 &position, float frequency,
                           float power, NodeType type) {
  int id = nextNodeId++;
  sources.emplace_back(id, position, frequency, power, type);
  return id;
}

void RadioSystem::removeSource(int id) {
  sources.erase(
      std::remove_if(sources.begin(), sources.end(),
                     [id](const RadioSource &s) { return s.id == id; }),
      sources.end());
}

void RadioSystem::removeSourceByIndex(int index) {
  if (index >= 0 && index < sources.size()) {
    sources.erase(sources.begin() + index);
  }
}

RadioSource *RadioSystem::getSourceById(int id) {
  for (auto &source : sources) {
    if (source.id == id) {
      return &source;
    }
  }
  return nullptr;
}

RadioSource *RadioSystem::getSourceByIndex(int index) {
  if (index >= 0 && index < sources.size()) {
    return &sources[index];
  }
  return nullptr;
}

void RadioSystem::update(float deltaTime) {}

float RadioSystem::calculatePathLoss(float distance, float frequency) {
  if (distance < 1.0f)
    distance = 1.0f;

  const float c = 299792458.0f;
  float wavelength = c / frequency;

  float fspl = 20.0f * log10(distance) + 20.0f * log10(frequency) - 147.55f;

  float loss = fspl / 100.0f;
  return expf(-loss);
}

float RadioSystem::calculateReflectionLoss(const glm::vec3 &normal) {
  return 0.3f;
}

std::vector<glm::vec3> RadioSystem::generateFibonacciSphere(int numSamples) {
  std::vector<glm::vec3> points;
  const float goldenRatio = (1.0f + std::sqrt(5.0f)) / 2.0f;
  const float angleIncrement = 2.0f * glm::pi<float>() * goldenRatio;

  for (int i = 0; i < numSamples; i++) {
    float t = (float)i / (float)numSamples;
    float inclination = std::acos(1.0f - 2.0f * t);
    float azimuth = angleIncrement * i;

    float x = std::sin(inclination) * std::cos(azimuth);
    float y = std::sin(inclination) * std::sin(azimuth);
    float z = std::cos(inclination);

    points.push_back(glm::vec3(x, y, z));
  }

  return points;
}

void RadioSystem::traceRay(const glm::vec3 &origin, const glm::vec3 &direction,
                           float strength, int bounce, const glm::vec3 &color,
                           const RadioSource &source,
                           const SpatialIndex *spatialIndex,
                           const std::vector<DiffractionEdge> &diffractionEdges,
                           SignalRay &currentPath) {
  if (bounce > maxBounces || strength < 0.01f) {
    return;
  }

  Ray testRay;
  testRay.origin = origin;
  testRay.direction = direction;
  testRay.tMin = 0.1f;
  testRay.tMax = maxDistance;

  RayHit hit = spatialIndex->intersect(testRay);

  if (hit.hit && hit.distance < maxDistance) {
    currentPath.points.push_back(hit.point);

    float distanceLoss = calculatePathLoss(hit.distance, source.frequency);
    float newStrength = strength * distanceLoss;

    if (enableDiffraction) {
      for (const auto &edge : diffractionEdges) {
        float distToEdgeStart = glm::length(edge.start - hit.point);
        float distToEdgeEnd = glm::length(edge.end - hit.point);
        glm::vec3 edgeMidpoint = (edge.start + edge.end) * 0.5f;
        float distToMidpoint = glm::length(edgeMidpoint - hit.point);

        if (distToMidpoint < 50.0f) {
          SignalRay diffractedRay;
          diffractedRay.origin = hit.point;
          diffractedRay.direction = glm::normalize(edgeMidpoint - hit.point);
          diffractedRay.strength = newStrength * 0.2f;
          diffractedRay.bounces = bounce + 1;
          diffractedRay.color = glm::vec3(1.0f, 1.0f, 0.0f);
          diffractedRay.points.push_back(hit.point);
          diffractedRay.points.push_back(edgeMidpoint);

          glm::vec3 edgeDir = glm::normalize(edge.end - edge.start);
          glm::vec3 incomingDir = direction;
          glm::vec3 perpDir = glm::normalize(glm::cross(edgeDir, incomingDir));
          if (glm::length(perpDir) < 0.1f) {
            perpDir = glm::normalize(glm::cross(edgeDir, glm::vec3(0, 1, 0)));
          }

          glm::vec3 diffractedDir = glm::normalize(incomingDir + perpDir * 0.5f);

          traceRay(edgeMidpoint, diffractedDir, diffractedRay.strength,
                   bounce + 1, diffractedRay.color, source, spatialIndex,
                   diffractionEdges, diffractedRay);

          if (diffractedRay.points.size() > 1) {
            signalRays.push_back(diffractedRay);
          }
        }
      }
    }

    if (bounce < maxBounces && newStrength > 0.01f) {
      float reflectionLoss = calculateReflectionLoss(hit.normal);
      float reflectedStrength = newStrength * reflectionLoss;

      glm::vec3 reflected = glm::reflect(direction, hit.normal);
      glm::vec3 newOrigin = hit.point + hit.normal * 0.1f;

      traceRay(newOrigin, reflected, reflectedStrength, bounce + 1, color,
               source, spatialIndex, diffractionEdges, currentPath);
    }
  } else {
    glm::vec3 endPoint = origin + direction * maxDistance;
    currentPath.points.push_back(endPoint);
  }
}

void RadioSystem::computeSignalPropagation(
    const SpatialIndex *spatialIndex,
    const std::vector<DiffractionEdge> &diffractionEdges) {
  signalRays.clear();

  if (!spatialIndex)
    return;

  for (const auto &source : sources) {
    if (!source.active || source.type != NodeType::TRANSMITTER)
      continue;

    std::vector<glm::vec3> directions = generateFibonacciSphere(raysPerSource);

    for (const auto &direction : directions) {
      SignalRay ray;
      ray.origin = source.position;
      ray.direction = direction;
      ray.strength = 1.0f;
      ray.bounces = 0;
      ray.color = source.color;
      ray.points.push_back(source.position);

      traceRay(source.position, direction, 1.0f, 0, source.color, source,
               spatialIndex, diffractionEdges, ray);

      if (ray.points.size() > 1) {
        signalRays.push_back(ray);
      }
    }
  }
}
