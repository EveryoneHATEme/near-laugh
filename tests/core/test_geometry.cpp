#include <gtest/gtest.h>

#include "src/core/math/geometry.hpp"

TEST(GeometryTest, RayHitsAabbInFront) {
  const math::Ray ray{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
  const math::AABB box{{-0.5f, 0.5f, -5.0f}, {0.5f, 1.5f, -4.0f}};

  const math::RayHit hit = math::intersectRay(ray, box);

  EXPECT_TRUE(hit.hit);
  EXPECT_NEAR(hit.distance, 4.0f, 0.001f);
}

TEST(GeometryTest, RayMissesAabbToTheSide) {
  const math::Ray ray{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
  const math::AABB box{{2.0f, 0.5f, -5.0f}, {3.0f, 1.5f, -4.0f}};

  const math::RayHit hit = math::intersectRay(ray, box);

  EXPECT_FALSE(hit.hit);
}
