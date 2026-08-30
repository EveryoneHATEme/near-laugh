#include "core/render/prototype_scene.hpp"

#include <array>

namespace {
using Point = std::array<float, 3>;
using Normal = std::array<float, 3>;
using Color = WorldColor;

void appendTriangle(std::vector<PositionColorVertex>& vertices, Point first,
                    Point second, Point third, Color color, Normal normal,
                    std::uint32_t solid_mask) {
  for (const Point point : {first, second, third}) {
    vertices.push_back({{point[0], point[1], point[2]},
                        {color[0], color[1], color[2], color[3]},
                        {normal[0], normal[1], normal[2]},
                        solid_mask});
  }
}

void appendQuad(std::vector<PositionColorVertex>& vertices, Point first,
                Point second, Point third, Point fourth, Color color,
                Normal normal, std::uint32_t solid_mask) {
  appendTriangle(vertices, first, second, third, color, normal, solid_mask);
  appendTriangle(vertices, first, third, fourth, color, normal, solid_mask);
}

void appendBox(std::vector<PositionColorVertex>& vertices, Point minimum,
               Point maximum, Color color, std::uint32_t solid_mask) {
  const float x0 = minimum[0];
  const float y0 = minimum[1];
  const float z0 = minimum[2];
  const float x1 = maximum[0];
  const float y1 = maximum[1];
  const float z1 = maximum[2];
  appendQuad(vertices, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
             color, {0.0F, 0.0F, 1.0F}, solid_mask);
  appendQuad(vertices, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
             color, {0.0F, 0.0F, -1.0F}, solid_mask);
  appendQuad(vertices, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0},
             color, {-1.0F, 0.0F, 0.0F}, solid_mask);
  appendQuad(vertices, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1},
             color, {1.0F, 0.0F, 0.0F}, solid_mask);
  appendQuad(vertices, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0},
             color, {0.0F, 1.0F, 0.0F}, solid_mask);
  appendQuad(vertices, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1},
             color, {0.0F, -1.0F, 0.0F}, solid_mask);
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
              std::uint32_t{1} << solid_index);
  }
  return vertices;
}
