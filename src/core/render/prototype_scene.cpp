#include "core/render/prototype_scene.hpp"

#include <array>
#include <cmath>

namespace {
using Point = std::array<float, 3>;
using Normal = std::array<float, 3>;
using TextureCoordinates = std::array<float, 2>;
using Color = WorldColor;

void appendTriangle(std::vector<PositionColorVertex>& vertices, Point first,
                    Point second, Point third, Color color, Normal normal,
                    TextureCoordinates first_uv, TextureCoordinates second_uv,
                    TextureCoordinates third_uv, std::uint32_t texture_layer) {
  const std::array<Point, 3> points = {first, second, third};
  const std::array<TextureCoordinates, 3> texture_coordinates = {
      first_uv, second_uv, third_uv};
  for (std::size_t index = 0; index < points.size(); ++index) {
    const Point point = points[index];
    const TextureCoordinates uv = texture_coordinates[index];
    vertices.push_back({{point[0], point[1], point[2]},
                        {color[0], color[1], color[2], color[3]},
                        {normal[0], normal[1], normal[2]},
                        {uv[0], uv[1]},
                        texture_layer});
  }
}

void appendQuad(std::vector<PositionColorVertex>& vertices, Point first,
                Point second, Point third, Point fourth, Color color,
                Normal normal, float u_extent, float v_extent,
                std::uint32_t texture_layer) {
  const TextureCoordinates bottom_left{0.0F, 0.0F};
  const TextureCoordinates bottom_right{u_extent, 0.0F};
  const TextureCoordinates top_right{u_extent, v_extent};
  const TextureCoordinates top_left{0.0F, v_extent};
  appendTriangle(vertices, first, second, third, color, normal, bottom_left,
                 bottom_right, top_right, texture_layer);
  appendTriangle(vertices, first, third, fourth, color, normal, bottom_left,
                 top_right, top_left, texture_layer);
}

void appendBox(std::vector<PositionColorVertex>& vertices, Point minimum,
               Point maximum, Color color, std::uint32_t texture_layer) {
  const float x0 = minimum[0];
  const float y0 = minimum[1];
  const float z0 = minimum[2];
  const float x1 = maximum[0];
  const float y1 = maximum[1];
  const float z1 = maximum[2];
  const float x_extent = x1 - x0;
  const float y_extent = y1 - y0;
  const float z_extent = z1 - z0;
  appendQuad(vertices, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
             color, {0.0F, 0.0F, 1.0F}, x_extent, y_extent, texture_layer);
  appendQuad(vertices, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
             color, {0.0F, 0.0F, -1.0F}, x_extent, y_extent, texture_layer);
  appendQuad(vertices, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0},
             color, {-1.0F, 0.0F, 0.0F}, z_extent, y_extent, texture_layer);
  appendQuad(vertices, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1},
             color, {1.0F, 0.0F, 0.0F}, z_extent, y_extent, texture_layer);
  appendQuad(vertices, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0},
             color, {0.0F, 1.0F, 0.0F}, x_extent, z_extent, texture_layer);
  appendQuad(vertices, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1},
             color, {0.0F, -1.0F, 0.0F}, x_extent, z_extent, texture_layer);
}

Normal triangleNormal(Point first, Point second, Point third) {
  const float first_x = second[0] - first[0];
  const float first_y = second[1] - first[1];
  const float first_z = second[2] - first[2];
  const float second_x = third[0] - first[0];
  const float second_y = third[1] - first[1];
  const float second_z = third[2] - first[2];
  const Normal normal{first_y * second_z - first_z * second_y,
                      first_z * second_x - first_x * second_z,
                      first_x * second_y - first_y * second_x};
  const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] +
                                 normal[2] * normal[2]);
  return {normal[0] / length, normal[1] / length, normal[2] / length};
}

void appendTerrain(std::vector<PositionColorVertex>& vertices,
                   const PrototypeTerrain& terrain) {
  constexpr Color terrain_color{86, 91, 101, 255};
  constexpr std::uint32_t terrain_layer =
      static_cast<std::uint32_t>(PrototypeSurface::Floor);
  for (std::size_t sample_z = 0; sample_z < prototype_terrain_cell_count;
       ++sample_z) {
    for (std::size_t sample_x = 0; sample_x < prototype_terrain_cell_count;
         ++sample_x) {
      const WorldPosition p00 =
          prototypeTerrainSamplePosition(terrain, sample_x, sample_z);
      const WorldPosition p01 =
          prototypeTerrainSamplePosition(terrain, sample_x, sample_z + 1);
      const WorldPosition p11 =
          prototypeTerrainSamplePosition(terrain, sample_x + 1, sample_z + 1);
      const WorldPosition p10 =
          prototypeTerrainSamplePosition(terrain, sample_x + 1, sample_z);
      const Point first{p00.x, p00.y, p00.z};
      const Point second{p01.x, p01.y, p01.z};
      const Point third{p11.x, p11.y, p11.z};
      const Point fourth{p10.x, p10.y, p10.z};
      appendTriangle(vertices, first, second, third, terrain_color,
                     triangleNormal(first, second, third), {first[0], first[2]},
                     {second[0], second[2]}, {third[0], third[2]},
                     terrain_layer);
      appendTriangle(vertices, first, third, fourth, terrain_color,
                     triangleNormal(first, third, fourth), {first[0], first[2]},
                     {third[0], third[2]}, {fourth[0], fourth[2]},
                     terrain_layer);
    }
  }
}

}  // namespace

std::vector<PositionColorVertex> buildPrototypeSceneVertices(
    const PrototypeLevel& level) {
  return buildPrototypeSceneVertices(level.terrain(), level.solids());
}

std::vector<PositionColorVertex> buildPrototypeSceneVertices(
    const PrototypeTerrain& terrain, std::span<const PrototypeSolid> solids) {
  std::vector<PositionColorVertex> vertices;
  vertices.reserve(solids.size() * 36 + prototype_terrain_cell_count *
                                            prototype_terrain_cell_count * 6);
  for (std::size_t solid_index = 0; solid_index < solids.size();
       ++solid_index) {
    const PrototypeSolid& solid = solids[solid_index];
    const Point minimum{solid.center.x - solid.half_extent.x,
                        solid.center.y - solid.half_extent.y,
                        solid.center.z - solid.half_extent.z};
    const Point maximum{solid.center.x + solid.half_extent.x,
                        solid.center.y + solid.half_extent.y,
                        solid.center.z + solid.half_extent.z};
    appendBox(vertices, minimum, maximum, solid.color,
              static_cast<std::uint32_t>(solid.surface));
  }
  appendTerrain(vertices, terrain);
  return vertices;
}
