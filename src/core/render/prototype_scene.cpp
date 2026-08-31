#include "core/render/prototype_scene.hpp"

#include <array>

namespace {
using Point = std::array<float, 3>;
using Normal = std::array<float, 3>;
using TextureCoordinates = std::array<float, 2>;
using Color = WorldColor;

void appendTriangle(std::vector<PositionColorVertex>& vertices, Point first,
                    Point second, Point third, Color color, Normal normal,
                    std::uint32_t solid_mask,
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
                        solid_mask,
                        {uv[0], uv[1]},
                        texture_layer});
  }
}

void appendQuad(std::vector<PositionColorVertex>& vertices, Point first,
                Point second, Point third, Point fourth, Color color,
                Normal normal, std::uint32_t solid_mask, float u_extent,
                float v_extent, std::uint32_t texture_layer) {
  const TextureCoordinates bottom_left{0.0F, 0.0F};
  const TextureCoordinates bottom_right{u_extent, 0.0F};
  const TextureCoordinates top_right{u_extent, v_extent};
  const TextureCoordinates top_left{0.0F, v_extent};
  appendTriangle(vertices, first, second, third, color, normal, solid_mask,
                 bottom_left, bottom_right, top_right, texture_layer);
  appendTriangle(vertices, first, third, fourth, color, normal, solid_mask,
                 bottom_left, top_right, top_left, texture_layer);
}

void appendBox(std::vector<PositionColorVertex>& vertices, Point minimum,
               Point maximum, Color color, std::uint32_t solid_mask,
               std::uint32_t texture_layer) {
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
             color, {0.0F, 0.0F, 1.0F}, solid_mask, x_extent, y_extent,
             texture_layer);
  appendQuad(vertices, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
             color, {0.0F, 0.0F, -1.0F}, solid_mask, x_extent, y_extent,
             texture_layer);
  appendQuad(vertices, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0},
             color, {-1.0F, 0.0F, 0.0F}, solid_mask, z_extent, y_extent,
             texture_layer);
  appendQuad(vertices, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1},
             color, {1.0F, 0.0F, 0.0F}, solid_mask, z_extent, y_extent,
             texture_layer);
  appendQuad(vertices, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0},
             color, {0.0F, 1.0F, 0.0F}, solid_mask, x_extent, z_extent,
             texture_layer);
  appendQuad(vertices, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1},
             color, {0.0F, -1.0F, 0.0F}, solid_mask, x_extent, z_extent,
             texture_layer);
}

}  // namespace

std::vector<PositionColorVertex> buildPrototypeSceneVertices(
    const PrototypeLevel& level) {
  std::vector<PositionColorVertex> vertices;
  vertices.reserve(level.solids().size() * 36);
  for (std::size_t solid_index = 0; solid_index < level.solids().size();
       ++solid_index) {
    const PrototypeSolid& solid = level.solids()[solid_index];
    const Point minimum{solid.center.x - solid.half_extent.x,
                        solid.center.y - solid.half_extent.y,
                        solid.center.z - solid.half_extent.z};
    const Point maximum{solid.center.x + solid.half_extent.x,
                        solid.center.y + solid.half_extent.y,
                        solid.center.z + solid.half_extent.z};
    appendBox(vertices, minimum, maximum, solid.color,
              std::uint32_t{1} << solid_index,
              static_cast<std::uint32_t>(solid.surface));
  }
  return vertices;
}
