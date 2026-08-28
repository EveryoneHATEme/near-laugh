#include <gtest/gtest.h>

#include <algorithm>
#include <array>
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
    return vertex.position[0] == x && vertex.position[1] == y &&
           vertex.position[2] == z;
  });
}
}  // namespace

TEST(PrototypeScene, ContainsFloorBoundariesAndMultipleColoredObjects) {
  const auto& vertices = prototypeSceneVertices();
  EXPECT_FALSE(vertices.empty());
  EXPECT_EQ(vertices.size() % 3, 0U);
  EXPECT_TRUE(hasColor(vertices, {86, 91, 101, 255}));
  EXPECT_TRUE(hasColor(vertices, {55, 78, 122, 255}));
  EXPECT_TRUE(hasColor(vertices, {205, 63, 73, 255}));
  EXPECT_TRUE(hasColor(vertices, {66, 176, 111, 255}));
  EXPECT_TRUE(hasColor(vertices, {225, 167, 62, 255}));
  EXPECT_TRUE(hasColor(vertices, {139, 91, 196, 255}));
}

TEST(PrototypeScene, CenterObjectsOverlapInDepthFromInitialPose) {
  const auto& vertices = prototypeSceneVertices();
  EXPECT_TRUE(hasPosition(vertices, -1.2F, 0.0F, 1.0F));
  EXPECT_TRUE(hasPosition(vertices, -1.5F, 0.0F, -4.5F));
  EXPECT_TRUE(hasPosition(vertices, 1.2F, 2.4F, -0.8F));
  EXPECT_TRUE(hasPosition(vertices, 1.5F, 3.0F, -6.5F));
}
