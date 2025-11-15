#include "spatial_index.h"
#include <algorithm>
#include <fstream>
#include <iostream>

SpatialIndex::SpatialIndex() {}
SpatialIndex::~SpatialIndex() {}

void SpatialIndex::build(const std::vector<Triangle> &triangles) {
  std::cout << "Building BVH..." << std::endl;
  m_triangles = triangles;

  if (m_triangles.empty())
    return;

  m_sceneBounds = BoundingBox();
  for (const auto &tri : m_triangles) {
    m_sceneBounds.expand(tri.v0);
    m_sceneBounds.expand(tri.v1);
    m_sceneBounds.expand(tri.v2);
  }

  std::vector<unsigned int> allIndices(m_triangles.size());
  for (size_t i = 0; i < m_triangles.size(); i++)
    allIndices[i] = i;

  m_root = buildBVH(allIndices, 0);
  std::cout << "BVH done" << std::endl;
}

std::unique_ptr<BVHNode>
SpatialIndex::buildBVH(std::vector<unsigned int> &indices, int depth) {
  auto node = std::make_unique<BVHNode>();

  node->bounds = BoundingBox();
  for (unsigned int idx : indices) {
    const Triangle &tri = m_triangles[idx];
    node->bounds.expand(tri.v0);
    node->bounds.expand(tri.v1);
    node->bounds.expand(tri.v2);
  }

  if (indices.size() <= 50 || depth >= 15) {
    node->isLeaf = true;
    node->triangleIndices = indices;
    return node;
  }

  glm::vec3 extent = node->bounds.max - node->bounds.min;
  int axis = (extent.x > extent.y && extent.x > extent.z)
                 ? 0
                 : (extent.y > extent.z ? 1 : 2);

  std::sort(indices.begin(), indices.end(),
            [&](unsigned int a, unsigned int b) {
              const Triangle &triA = m_triangles[a];
              const Triangle &triB = m_triangles[b];
              glm::vec3 centroidA = (triA.v0 + triA.v1 + triA.v2) * 0.333333f;
              glm::vec3 centroidB = (triB.v0 + triB.v1 + triB.v2) * 0.333333f;
              return centroidA[axis] < centroidB[axis];
            });

  size_t mid = indices.size() / 2;
  std::vector<unsigned int> leftIndices(indices.begin(), indices.begin() + mid);
  std::vector<unsigned int> rightIndices(indices.begin() + mid, indices.end());

  if (!leftIndices.empty())
    node->left = buildBVH(leftIndices, depth + 1);
  if (!rightIndices.empty())
    node->right = buildBVH(rightIndices, depth + 1);

  return node;
}

RayHit SpatialIndex::intersect(const Ray &ray) const {
  if (!m_root)
    return RayHit();
  return intersectBVH(m_root.get(), ray);
}

RayHit SpatialIndex::intersectBVH(const BVHNode *node, const Ray &ray) const {
  if (!node ||
      !node->bounds.intersect(ray.origin, ray.direction, ray.tMin, ray.tMax)) {
    return RayHit();
  }

  if (node->isLeaf) {
    RayHit closestHit;
    closestHit.hit = false;
    closestHit.distance = ray.tMax;

    for (unsigned int idx : node->triangleIndices) {
      const Triangle &tri = m_triangles[idx];
      float t;
      glm::vec3 hitPoint;

      if (intersectTriangle(ray, tri, t, hitPoint) && t > ray.tMin &&
          t < closestHit.distance) {
        closestHit.hit = true;
        closestHit.distance = t;
        closestHit.point = hitPoint;
        closestHit.normal = tri.normal;
        closestHit.triangleId = tri.id;
      }
    }
    return closestHit;
  }

  RayHit leftHit = intersectBVH(node->left.get(), ray);
  RayHit rightHit = intersectBVH(node->right.get(), ray);

  if (leftHit.hit && rightHit.hit) {
    return (leftHit.distance < rightHit.distance) ? leftHit : rightHit;
  }
  return leftHit.hit ? leftHit : rightHit;
}

bool SpatialIndex::intersectAny(const Ray &ray) const {
  if (!m_root)
    return false;
  return intersectAnyBVH(m_root.get(), ray);
}

bool SpatialIndex::intersectAnyBVH(const BVHNode *node, const Ray &ray) const {
  if (!node ||
      !node->bounds.intersect(ray.origin, ray.direction, ray.tMin, ray.tMax))
    return false;

  if (node->isLeaf) {
    for (unsigned int idx : node->triangleIndices) {
      const Triangle &tri = m_triangles[idx];
      float t;
      glm::vec3 hitPoint;
      if (intersectTriangle(ray, tri, t, hitPoint) && t > ray.tMin &&
          t < ray.tMax)
        return true;
    }
    return false;
  }

  return intersectAnyBVH(node->left.get(), ray) ||
         intersectAnyBVH(node->right.get(), ray);
}

bool SpatialIndex::intersectTriangle(const Ray &ray, const Triangle &tri,
                                     float &t, glm::vec3 &hitPoint) const {
  const float EPSILON = 0.0000001f;

  glm::vec3 edge1 = tri.v1 - tri.v0;
  glm::vec3 edge2 = tri.v2 - tri.v0;
  glm::vec3 h = glm::cross(ray.direction, edge2);
  float a = glm::dot(edge1, h);

  if (a > -EPSILON && a < EPSILON)
    return false;

  float f = 1.0f / a;
  glm::vec3 s = ray.origin - tri.v0;
  float u = f * glm::dot(s, h);

  if (u < 0.0f || u > 1.0f)
    return false;

  glm::vec3 q = glm::cross(s, edge1);
  float v = f * glm::dot(ray.direction, q);

  if (v < 0.0f || u + v > 1.0f)
    return false;

  t = f * glm::dot(edge2, q);

  if (t > EPSILON) {
    hitPoint = ray.origin + ray.direction * t;
    return true;
  }

  return false;
}

void SpatialIndex::extractBuildings() {
  std::cout << "Skipping building extraction (not needed for physics)"
            << std::endl;
}

void SpatialIndex::printStats() const {
  glm::vec3 size = m_sceneBounds.max - m_sceneBounds.min;
  std::cout << "Spatial index: " << m_triangles.size() << " triangles, "
            << (int)size.x << "x" << (int)size.y << "x" << (int)size.z << "m\n"
            << std::endl;
}

bool SpatialIndex::saveBVH(const std::string &filename) const {
  std::ofstream out(filename, std::ios::binary);
  if (!out.is_open()) {
    std::cerr << "Failed to open file for writing: " << filename << std::endl;
    return false;
  }

  const char magic[4] = {'B', 'V', 'H', '1'};
  out.write(magic, 4);

  size_t triCount = m_triangles.size();
  out.write(reinterpret_cast<const char *>(&triCount), sizeof(size_t));

  for (const auto &tri : m_triangles) {
    out.write(reinterpret_cast<const char *>(&tri.v0), sizeof(glm::vec3));
    out.write(reinterpret_cast<const char *>(&tri.v1), sizeof(glm::vec3));
    out.write(reinterpret_cast<const char *>(&tri.v2), sizeof(glm::vec3));
    out.write(reinterpret_cast<const char *>(&tri.normal), sizeof(glm::vec3));
    out.write(reinterpret_cast<const char *>(&tri.id), sizeof(unsigned int));
  }

  out.write(reinterpret_cast<const char *>(&m_sceneBounds.min),
            sizeof(glm::vec3));
  out.write(reinterpret_cast<const char *>(&m_sceneBounds.max),
            sizeof(glm::vec3));

  serializeBVHNode(out, m_root.get());

  out.close();
  std::cout << "BVH saved to " << filename << std::endl;
  return true;
}

bool SpatialIndex::loadBVH(const std::string &filename) {
  std::ifstream in(filename, std::ios::binary);
  if (!in.is_open()) {
    std::cerr << "BVH file not found: " << filename << std::endl;
    return false;
  }

  char magic[4];
  in.read(magic, 4);
  if (magic[0] != 'B' || magic[1] != 'V' || magic[2] != 'H' ||
      magic[3] != '1') {
    std::cerr << "Invalid BVH file format" << std::endl;
    return false;
  }

  size_t triCount;
  in.read(reinterpret_cast<char *>(&triCount), sizeof(size_t));

  m_triangles.clear();
  m_triangles.reserve(triCount);
  for (size_t i = 0; i < triCount; i++) {
    Triangle tri;
    in.read(reinterpret_cast<char *>(&tri.v0), sizeof(glm::vec3));
    in.read(reinterpret_cast<char *>(&tri.v1), sizeof(glm::vec3));
    in.read(reinterpret_cast<char *>(&tri.v2), sizeof(glm::vec3));
    in.read(reinterpret_cast<char *>(&tri.normal), sizeof(glm::vec3));
    in.read(reinterpret_cast<char *>(&tri.id), sizeof(unsigned int));
    m_triangles.push_back(tri);
  }

  in.read(reinterpret_cast<char *>(&m_sceneBounds.min), sizeof(glm::vec3));
  in.read(reinterpret_cast<char *>(&m_sceneBounds.max), sizeof(glm::vec3));

  m_root = deserializeBVHNode(in);

  in.close();
  std::cout << "BVH loaded from " << filename << " (" << triCount
            << " triangles)" << std::endl;
  return true;
}

void SpatialIndex::serializeBVHNode(std::ofstream &out,
                                    const BVHNode *node) const {
  if (!node) {
    bool isNull = true;
    out.write(reinterpret_cast<const char *>(&isNull), sizeof(bool));
    return;
  }

  bool isNull = false;
  out.write(reinterpret_cast<const char *>(&isNull), sizeof(bool));

  out.write(reinterpret_cast<const char *>(&node->bounds.min),
            sizeof(glm::vec3));
  out.write(reinterpret_cast<const char *>(&node->bounds.max),
            sizeof(glm::vec3));

  out.write(reinterpret_cast<const char *>(&node->isLeaf), sizeof(bool));

  if (node->isLeaf) {
    size_t indexCount = node->triangleIndices.size();
    out.write(reinterpret_cast<const char *>(&indexCount), sizeof(size_t));
    out.write(reinterpret_cast<const char *>(node->triangleIndices.data()),
              indexCount * sizeof(unsigned int));
  } else {
    serializeBVHNode(out, node->left.get());
    serializeBVHNode(out, node->right.get());
  }
}

std::unique_ptr<BVHNode> SpatialIndex::deserializeBVHNode(std::ifstream &in) {
  bool isNull;
  in.read(reinterpret_cast<char *>(&isNull), sizeof(bool));
  if (isNull) {
    return nullptr;
  }

  auto node = std::make_unique<BVHNode>();

  in.read(reinterpret_cast<char *>(&node->bounds.min), sizeof(glm::vec3));
  in.read(reinterpret_cast<char *>(&node->bounds.max), sizeof(glm::vec3));

  in.read(reinterpret_cast<char *>(&node->isLeaf), sizeof(bool));

  if (node->isLeaf) {
    size_t indexCount;
    in.read(reinterpret_cast<char *>(&indexCount), sizeof(size_t));
    node->triangleIndices.resize(indexCount);
    in.read(reinterpret_cast<char *>(node->triangleIndices.data()),
            indexCount * sizeof(unsigned int));
  } else {
    node->left = deserializeBVHNode(in);
    node->right = deserializeBVHNode(in);
  }

  return node;
}
