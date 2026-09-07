#ifndef CORE_RENDER_POINT_SHADOW_HPP
#define CORE_RENDER_POINT_SHADOW_HPP

#include <array>
#include <cstdint>

#include "core/frame.hpp"
#include "core/world/level_document.hpp"

inline constexpr std::uint32_t point_shadow_face_size = 512;
inline constexpr float point_shadow_near = .01F;
inline constexpr std::size_t lighting_frame_slot_count = 2;

// Face order +X, -X, +Y, -Y, +Z, -Z. Positive viewport height means
// the second basis vector points down in the depth image.
struct PointShadowBasis {
  WorldPosition forward, right, down;
};
inline constexpr std::array<PointShadowBasis, 6> point_shadow_bases{
    {{{1, 0, 0}, {0, 0, -1}, {0, -1, 0}},
     {{-1, 0, 0}, {0, 0, 1}, {0, -1, 0}},
     {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}},
     {{0, -1, 0}, {1, 0, 0}, {0, 0, -1}},
     {{0, 0, 1}, {1, 0, 0}, {0, -1, 0}},
     {{0, 0, -1}, {-1, 0, 0}, {0, -1, 0}}}};

[[nodiscard]] CameraFrame pointShadowCamera(WorldPosition source, float radius,
                                            std::size_t face);
struct PointShadowProjection {
  std::size_t face;
  float u, v, depth;
};
[[nodiscard]] PointShadowProjection projectPointShadow(WorldPosition direction,
                                                       float radius);
[[nodiscard]] WorldPosition pointShadowDirection(std::size_t face, float u,
                                                 float v);

#endif
