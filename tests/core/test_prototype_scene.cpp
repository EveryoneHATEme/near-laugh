#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "core/render/prototype_scene.hpp"

namespace {
bool hasColor(const std::vector<PositionColorVertex>& vertices,
              std::array<std::uint8_t, 4> color) {
  return std::any_of(vertices.begin(), vertices.end(), [&](const auto& vertex) {
    return std::equal(std::begin(vertex.color), std::end(vertex.color),
                      color.begin());
  });
}

bool hasPosition(const std::vector<PositionColorVertex>& vertices, float x,
                 float y, float z) {
  return std::any_of(vertices.begin(), vertices.end(), [&](const auto& vertex) {
    return std::abs(vertex.position[0] - x) < 0.0001F &&
           std::abs(vertex.position[1] - y) < 0.0001F &&
           std::abs(vertex.position[2] - z) < 0.0001F;
  });
}
}  // namespace

TEST(PrototypeScene, ContainsFloorBoundariesAndMultipleColoredObjects) {
  const PrototypeLevel level;
  const auto vertices = buildPrototypeSceneVertices(level);
  EXPECT_FALSE(vertices.empty());
  EXPECT_EQ(vertices.size() % 3, 0U);
  EXPECT_EQ(vertices.size(), level.solids().size() * 36U);
  EXPECT_TRUE(hasColor(vertices, {86, 91, 101, 255}));
  EXPECT_TRUE(hasColor(vertices, {55, 78, 122, 255}));
  EXPECT_TRUE(hasColor(vertices, {205, 63, 73, 255}));
  EXPECT_TRUE(hasColor(vertices, {66, 176, 111, 255}));
  EXPECT_TRUE(hasColor(vertices, {225, 167, 62, 255}));
  EXPECT_TRUE(hasColor(vertices, {139, 91, 196, 255}));
  EXPECT_TRUE(hasColor(vertices, {70, 184, 190, 255}));
  EXPECT_TRUE(hasColor(vertices, {190, 118, 197, 255}));
}

TEST(PrototypeScene, CenterObjectsOverlapInDepthFromInitialPose) {
  const auto vertices = buildPrototypeSceneVertices(PrototypeLevel{});
  EXPECT_TRUE(hasPosition(vertices, -1.2F, 0.0F, 1.0F));
  EXPECT_TRUE(hasPosition(vertices, -1.5F, 0.0F, -4.5F));
  EXPECT_TRUE(hasPosition(vertices, 1.2F, 2.4F, -0.8F));
  EXPECT_TRUE(hasPosition(vertices, 1.5F, 3.0F, -6.5F));
}

TEST(PrototypeScene, ExpandsEverySolidFaceAtItsAuthoredBoundsAndColor) {
  const PrototypeLevel level;
  const auto vertices = buildPrototypeSceneVertices(level);
  for (const PrototypeSolid& solid : level.solids()) {
    const float minimum_x = solid.center.x - solid.half_extent.x;
    const float minimum_y = solid.center.y - solid.half_extent.y;
    const float minimum_z = solid.center.z - solid.half_extent.z;
    const float maximum_x = solid.center.x + solid.half_extent.x;
    const float maximum_y = solid.center.y + solid.half_extent.y;
    const float maximum_z = solid.center.z + solid.half_extent.z;
    EXPECT_TRUE(hasPosition(vertices, minimum_x, minimum_y, minimum_z));
    EXPECT_TRUE(hasPosition(vertices, maximum_x, maximum_y, maximum_z));
    EXPECT_TRUE(hasColor(vertices, solid.color));
  }
}
