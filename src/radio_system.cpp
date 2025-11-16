#include "radio_system.h"
#include "spatial_index.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

RadioSystem::RadioSystem()
    : nextNodeId(1), raysPerSource(64), maxBounces(2), maxDistance(2000.0f) {}

RadioSystem::~RadioSystem() {}

int RadioSystem::addSource(const glm::vec3 &position, float frequency,
                           NodeType type) {
  int id = nextNodeId++;
  sources.emplace_back(id, position, frequency, type);
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

void RadioSystem::computeSignalPropagation(const SpatialIndex *spatialIndex) {
  signalRays.clear();

  if (!spatialIndex)
    return;

  for (const auto &source : sources) {
    if (!source.active)
      continue;

    for (int i = 0; i < raysPerSource; i++) {
      float theta = 2.0f * glm::pi<float>() * (i / (float)raysPerSource);
      float phi = glm::pi<float>() * (0.5f + 0.4f * sin(theta * 3.0f));

      glm::vec3 direction(sin(phi) * cos(theta), cos(phi),
                          sin(phi) * sin(theta));

      SignalRay ray;
      ray.origin = source.position;
      ray.direction = glm::normalize(direction);
      ray.strength = 1.0f;
      ray.bounces = 0;
      ray.color = source.color;
      ray.points.push_back(source.position);

      glm::vec3 currentPos = source.position;
      glm::vec3 currentDir = ray.direction;
      float currentStrength = 1.0f;

      for (int bounce = 0; bounce <= maxBounces; bounce++) {
        Ray testRay;
        testRay.origin = currentPos;
        testRay.direction = currentDir;
        testRay.tMin = 0.1f;
        testRay.tMax = maxDistance;

        RayHit hit = spatialIndex->intersect(testRay);

        if (hit.hit && hit.distance < maxDistance) {
          glm::vec3 hitPoint = hit.point;
          ray.points.push_back(hitPoint);

          float distanceLoss =
              calculatePathLoss(hit.distance, source.frequency);
          currentStrength *= distanceLoss;

          if (bounce < maxBounces && currentStrength > 0.01f) {
            float reflectionLoss = calculateReflectionLoss(hit.normal);
            currentStrength *= reflectionLoss;

            glm::vec3 reflected = glm::reflect(currentDir, hit.normal);
            currentDir = reflected;
            currentPos = hitPoint + hit.normal * 0.1f;
          } else {
            break;
          }
        } else {
          glm::vec3 endPoint = currentPos + currentDir * maxDistance;
          ray.points.push_back(endPoint);

          float distanceLoss = calculatePathLoss(maxDistance, source.frequency);
          currentStrength *= distanceLoss;
          break;
        }
      }

      ray.strength = currentStrength;
      ray.bounces = ray.points.size() - 1;

      if (ray.points.size() > 1) {
        signalRays.push_back(ray);
      }
    }
  }
}
