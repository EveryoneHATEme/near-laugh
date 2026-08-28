#include "core/render/prototype_scene.hpp"

#include <array>

namespace {
using Point = std::array<float, 3>;
using Color = std::array<std::uint8_t, 4>;

void appendTriangle(std::vector<PositionColorVertex>& vertices, Point first,
                    Point second, Point third, Color color) {
  for (const Point point : {first, second, third}) {
    vertices.push_back({{point[0], point[1], point[2]},
                        {color[0], color[1], color[2], color[3]}});
  }
}

void appendQuad(std::vector<PositionColorVertex>& vertices, Point first,
                Point second, Point third, Point fourth, Color color) {
  appendTriangle(vertices, first, second, third, color);
  appendTriangle(vertices, first, third, fourth, color);
}

void appendBox(std::vector<PositionColorVertex>& vertices, Point minimum,
               Point maximum, Color color) {
  const float x0 = minimum[0];
  const float y0 = minimum[1];
  const float z0 = minimum[2];
  const float x1 = maximum[0];
  const float y1 = maximum[1];
  const float z1 = maximum[2];
  appendQuad(vertices, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
             color);
  appendQuad(vertices, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
             color);
  appendQuad(vertices, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0},
             color);
  appendQuad(vertices, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1},
             color);
  appendQuad(vertices, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0},
             color);
  appendQuad(vertices, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1},
             color);
}

std::vector<PositionColorVertex> buildPrototypeScene() {
  constexpr Color floor_color{86, 91, 101, 255};
  constexpr Color boundary_color{55, 78, 122, 255};
  constexpr Color red{205, 63, 73, 255};
  constexpr Color green{66, 176, 111, 255};
  constexpr Color gold{225, 167, 62, 255};
  constexpr Color violet{139, 91, 196, 255};

  std::vector<PositionColorVertex> vertices;
  vertices.reserve(168);
  appendQuad(vertices, {-10.0F, 0.0F, 4.0F}, {10.0F, 0.0F, 4.0F},
             {10.0F, 0.0F, -14.0F}, {-10.0F, 0.0F, -14.0F}, floor_color);
  appendQuad(vertices, {-10.0F, 0.0F, -14.0F}, {10.0F, 0.0F, -14.0F},
             {10.0F, 5.0F, -14.0F}, {-10.0F, 5.0F, -14.0F}, boundary_color);
  appendQuad(vertices, {-10.0F, 0.0F, 4.0F}, {-10.0F, 0.0F, -14.0F},
             {-10.0F, 5.0F, -14.0F}, {-10.0F, 5.0F, 4.0F}, boundary_color);
  appendQuad(vertices, {10.0F, 0.0F, -14.0F}, {10.0F, 0.0F, 4.0F},
             {10.0F, 5.0F, 4.0F}, {10.0F, 5.0F, -14.0F}, boundary_color);

  // Two center-line boxes deliberately overlap from the initial camera pose.
  appendBox(vertices, {-1.2F, 0.0F, -0.8F}, {1.2F, 2.4F, 1.0F}, red);
  appendBox(vertices, {-1.5F, 0.0F, -6.5F}, {1.5F, 3.0F, -4.5F}, green);
  appendBox(vertices, {-5.5F, 0.0F, -4.5F}, {-3.5F, 4.0F, -2.5F}, gold);
  appendBox(vertices, {3.5F, 0.0F, -9.5F}, {5.5F, 2.0F, -7.5F}, violet);
  return vertices;
}
}  // namespace

const std::vector<PositionColorVertex>& prototypeSceneVertices() {
  static const std::vector<PositionColorVertex> vertices =
      buildPrototypeScene();
  return vertices;
}
