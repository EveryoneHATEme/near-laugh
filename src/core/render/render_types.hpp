#ifndef CORE_RENDER_RENDER_TYPES_HPP
#define CORE_RENDER_RENDER_TYPES_HPP

#include <cstdint>
#include <vector>

struct Color {
  uint8_t r{255};
  uint8_t g{255};
  uint8_t b{255};
  uint8_t a{255};
};

struct PositionColorVertex {
  float position[3]{};
  uint8_t color[4]{};
};

struct RenderPacket {
  std::vector<PositionColorVertex> vertices;
  std::vector<uint16_t> indices;

  bool empty() const { return indices.empty(); }
  void clear() {
    vertices.clear();
    indices.clear();
  }
};

inline PositionColorVertex makePositionColorVertex(float x, float y, float z,
                                                   Color color) {
  return {{x, y, z}, {color.r, color.g, color.b, color.a}};
}

#endif
