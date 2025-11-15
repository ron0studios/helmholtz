#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>

struct Triangle {
  glm::vec3 v0, v1, v2;
  glm::vec3 normal;
  unsigned int id;
};

struct BoundingBox {
  glm::vec3 min;
  glm::vec3 max;

  BoundingBox() : min(glm::vec3(FLT_MAX)), max(glm::vec3(-FLT_MAX)) {}
  BoundingBox(const glm::vec3 &min, const glm::vec3 &max)
      : min(min), max(max) {}

  void expand(const glm::vec3 &point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
  }

  void expand(const BoundingBox &box) {
    min = glm::min(min, box.min);
    max = glm::max(max, box.max);
  }

  glm::vec3 centroid() const { return (min + max) * 0.5f; }

  float surfaceArea() const {
    glm::vec3 d = max - min;
    return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
  }

  bool intersect(const glm::vec3 &origin, const glm::vec3 &direction,
                 float tMin, float tMax) const {
    for (int i = 0; i < 3; i++) {
      float invD = 1.0f / direction[i];
      float t0 = (min[i] - origin[i]) * invD;
      float t1 = (max[i] - origin[i]) * invD;
      if (invD < 0.0f)
        std::swap(t0, t1);
      tMin = t0 > tMin ? t0 : tMin;
      tMax = t1 < tMax ? t1 : tMax;
      if (tMax <= tMin)
        return false;
    }
    return true;
  }
};

struct Ray {
  glm::vec3 origin;
  glm::vec3 direction;
  float tMin = 0.001f;
  float tMax = 10000.0f;
};

struct RayHit {
  bool hit = false;
  float distance = FLT_MAX;
  glm::vec3 point;
  glm::vec3 normal;
  unsigned int triangleId = 0;
};

struct BVHNode {
  BoundingBox bounds;
  std::unique_ptr<BVHNode> left;
  std::unique_ptr<BVHNode> right;
  std::vector<unsigned int> triangleIndices;
  bool isLeaf = false;
};

struct Building {
  std::vector<unsigned int> triangleIndices;
  BoundingBox bounds;
  glm::vec3 centroid;
  float height;
};

class SpatialIndex {
public:
  SpatialIndex();
  ~SpatialIndex();

  void build(const std::vector<Triangle> &triangles);
  RayHit intersect(const Ray &ray) const;
  bool intersectAny(const Ray &ray) const;

  // Serialization
  bool saveBVH(const std::string &filename) const;
  bool loadBVH(const std::string &filename);

  const std::vector<Triangle> &getTriangles() const { return m_triangles; }
  const BoundingBox &getBounds() const { return m_sceneBounds; }
  const std::vector<Building> &getBuildings() const { return m_buildings; }

  void extractBuildings();
  void printStats() const;

private:
  std::vector<Triangle> m_triangles;
  std::unique_ptr<BVHNode> m_root;
  BoundingBox m_sceneBounds;
  std::vector<Building> m_buildings;

  std::unique_ptr<BVHNode> buildBVH(std::vector<unsigned int> &triangleIndices,
                                    int depth);
  RayHit intersectBVH(const BVHNode *node, const Ray &ray) const;
  bool intersectAnyBVH(const BVHNode *node, const Ray &ray) const;
  bool intersectTriangle(const Ray &ray, const Triangle &tri, float &t,
                         glm::vec3 &hitPoint) const;

  // Serialization helpers
  void serializeBVHNode(std::ofstream &out, const BVHNode *node) const;
  std::unique_ptr<BVHNode> deserializeBVHNode(std::ifstream &in);
};
