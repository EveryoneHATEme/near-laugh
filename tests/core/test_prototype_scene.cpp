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

bool samePositionAndUv(const PositionColorVertex& first,
                       const PositionColorVertex& second) {
  return std::equal(std::begin(first.position), std::end(first.position),
                    std::begin(second.position)) &&
         std::equal(std::begin(first.texture_coordinates),
                    std::end(first.texture_coordinates),
                    std::begin(second.texture_coordinates));
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

TEST(PrototypeScene, GivesEveryFaceAConsistentOutwardUnitNormal) {
  const PrototypeLevel level;
  const auto vertices = buildPrototypeSceneVertices(level);
  constexpr std::size_t vertices_per_solid = 36;
  constexpr std::size_t vertices_per_face = 6;

  for (std::size_t solid_index = 0; solid_index < level.solids().size();
       ++solid_index) {
    const PrototypeSolid& solid = level.solids()[solid_index];
    for (std::size_t face_index = 0; face_index < 6; ++face_index) {
      const std::size_t first_index =
          solid_index * vertices_per_solid + face_index * vertices_per_face;
      const PositionColorVertex& first = vertices[first_index];
      const float length_squared = first.normal[0] * first.normal[0] +
                                   first.normal[1] * first.normal[1] +
                                   first.normal[2] * first.normal[2];
      EXPECT_FLOAT_EQ(length_squared, 1.0F);

      std::array<float, 3> face_center{};
      for (std::size_t vertex_index = 0; vertex_index < vertices_per_face;
           ++vertex_index) {
        const PositionColorVertex& vertex =
            vertices[first_index + vertex_index];
        EXPECT_FLOAT_EQ(vertex.normal[0], first.normal[0]);
        EXPECT_FLOAT_EQ(vertex.normal[1], first.normal[1]);
        EXPECT_FLOAT_EQ(vertex.normal[2], first.normal[2]);
        for (std::size_t axis = 0; axis < face_center.size(); ++axis) {
          face_center[axis] +=
              vertex.position[axis] / static_cast<float>(vertices_per_face);
        }
      }

      const float outward_dot =
          first.normal[0] * (face_center[0] - solid.center.x) +
          first.normal[1] * (face_center[1] - solid.center.y) +
          first.normal[2] * (face_center[2] - solid.center.z);
      EXPECT_GT(outward_dot, 0.0F);
      const float expected_distance =
          std::abs(first.normal[0]) * solid.half_extent.x +
          std::abs(first.normal[1]) * solid.half_extent.y +
          std::abs(first.normal[2]) * solid.half_extent.z;
      EXPECT_NEAR(outward_dot, expected_distance, 0.00001F);
    }
  }
}

TEST(PrototypeScene, GivesEveryFaceFiniteContinuousWorldScaledCoordinates) {
  const PrototypeLevel level;
  const auto vertices = buildPrototypeSceneVertices(level);
  constexpr std::size_t vertices_per_solid = 36;
  constexpr std::size_t vertices_per_face = 6;

  for (std::size_t solid_index = 0; solid_index < level.solids().size();
       ++solid_index) {
    const PrototypeSolid& solid = level.solids()[solid_index];
    const std::array<std::array<float, 2>, 6> face_extents = {{
        {solid.half_extent.x * 2.0F, solid.half_extent.y * 2.0F},
        {solid.half_extent.x * 2.0F, solid.half_extent.y * 2.0F},
        {solid.half_extent.z * 2.0F, solid.half_extent.y * 2.0F},
        {solid.half_extent.z * 2.0F, solid.half_extent.y * 2.0F},
        {solid.half_extent.x * 2.0F, solid.half_extent.z * 2.0F},
        {solid.half_extent.x * 2.0F, solid.half_extent.z * 2.0F},
    }};
    for (std::size_t face = 0; face < face_extents.size(); ++face) {
      const std::size_t base =
          solid_index * vertices_per_solid + face * vertices_per_face;
      for (std::size_t vertex = 0; vertex < vertices_per_face; ++vertex) {
        EXPECT_TRUE(
            std::isfinite(vertices[base + vertex].texture_coordinates[0]));
        EXPECT_TRUE(
            std::isfinite(vertices[base + vertex].texture_coordinates[1]));
      }
      EXPECT_TRUE(samePositionAndUv(vertices[base], vertices[base + 3]));
      EXPECT_TRUE(samePositionAndUv(vertices[base + 2], vertices[base + 4]));
      EXPECT_FLOAT_EQ(vertices[base].texture_coordinates[0], 0.0F);
      EXPECT_FLOAT_EQ(vertices[base].texture_coordinates[1], 0.0F);
      EXPECT_NEAR(vertices[base + 1].texture_coordinates[0],
                  face_extents[face][0], 0.000001F);
      EXPECT_FLOAT_EQ(vertices[base + 1].texture_coordinates[1], 0.0F);
      EXPECT_NEAR(vertices[base + 2].texture_coordinates[0],
                  face_extents[face][0], 0.000001F);
      EXPECT_NEAR(vertices[base + 2].texture_coordinates[1],
                  face_extents[face][1], 0.000001F);
      EXPECT_FLOAT_EQ(vertices[base + 5].texture_coordinates[0], 0.0F);
      EXPECT_NEAR(vertices[base + 5].texture_coordinates[1],
                  face_extents[face][1], 0.000001F);
    }
  }
}

TEST(PrototypeScene, FaceTextureAxesFollowEachOutwardNormal) {
  const auto vertices = buildPrototypeSceneVertices(PrototypeLevel{});
  constexpr std::size_t vertices_per_face = 6;
  for (std::size_t face = 0; face < 6; ++face) {
    const PositionColorVertex& origin = vertices[face * vertices_per_face];
    const PositionColorVertex& along_u = vertices[face * vertices_per_face + 1];
    const PositionColorVertex& along_uv =
        vertices[face * vertices_per_face + 2];
    const std::array<float, 3> u_axis = {
        along_u.position[0] - origin.position[0],
        along_u.position[1] - origin.position[1],
        along_u.position[2] - origin.position[2]};
    const std::array<float, 3> v_axis = {
        along_uv.position[0] - along_u.position[0],
        along_uv.position[1] - along_u.position[1],
        along_uv.position[2] - along_u.position[2]};
    const std::array<float, 3> cross = {
        u_axis[1] * v_axis[2] - u_axis[2] * v_axis[1],
        u_axis[2] * v_axis[0] - u_axis[0] * v_axis[2],
        u_axis[0] * v_axis[1] - u_axis[1] * v_axis[0]};
    const float orientation = cross[0] * origin.normal[0] +
                              cross[1] * origin.normal[1] +
                              cross[2] * origin.normal[2];
    EXPECT_GT(orientation, 0.0F);
  }
}

TEST(PrototypeScene, LargeSurfacesRepeatAndLayersMatchStableSurfaceRoles) {
  const PrototypeLevel level;
  const auto vertices = buildPrototypeSceneVertices(level);
  constexpr std::size_t vertices_per_solid = 36;
  constexpr std::size_t floor_top_face_first_vertex = 4 * 6;
  EXPECT_GT(vertices[floor_top_face_first_vertex + 1].texture_coordinates[0],
            1.0F);
  EXPECT_GT(vertices[floor_top_face_first_vertex + 2].texture_coordinates[1],
            1.0F);

  for (std::size_t solid_index = 0; solid_index < level.solids().size();
       ++solid_index) {
    const std::uint32_t expected_layer =
        static_cast<std::uint32_t>(level.solids()[solid_index].surface);
    for (std::size_t vertex = 0; vertex < vertices_per_solid; ++vertex) {
      EXPECT_EQ(
          vertices[solid_index * vertices_per_solid + vertex].texture_layer,
          expected_layer);
    }
  }
}
