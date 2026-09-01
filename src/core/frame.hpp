#ifndef CORE_FRAME_HPP
#define CORE_FRAME_HPP

#include <array>
#include <cmath>
#include <cstdint>
#include <type_traits>

struct FramebufferExtent {
  std::uint32_t width{};
  std::uint32_t height{};

  [[nodiscard]] constexpr bool isZero() const noexcept {
    return width == 0 || height == 0;
  }
};

// Column-major view-projection matrix. The fixed scalar storage keeps the
// runtime-to-render boundary independent of math and graphics libraries.
struct CameraFrame {
  std::array<float, 16> view_projection{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                        0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                        0.0F, 0.0F, 0.0F, 1.0F};
};

static_assert(std::is_standard_layout_v<CameraFrame>);
static_assert(sizeof(CameraFrame) == sizeof(float) * 16);

struct alignas(16) SpotLightFrame {
  std::array<float, 4> position_and_range{};
  std::array<float, 4> direction_and_inner_cosine{};
  std::array<float, 4> color_and_intensity{};
  std::array<float, 4> outer_cosine_and_enabled{};
};

static_assert(std::is_standard_layout_v<SpotLightFrame>);
static_assert(alignof(SpotLightFrame) == 16);
static_assert(sizeof(SpotLightFrame) == sizeof(float) * 16);

[[nodiscard]] inline bool spotLightFrameIsValid(
    const SpotLightFrame& light) noexcept {
  const auto finite = [](const std::array<float, 4>& values) {
    for (const float value : values) {
      if (!std::isfinite(value)) {
        return false;
      }
    }
    return true;
  };
  if (!finite(light.position_and_range) ||
      !finite(light.direction_and_inner_cosine) ||
      !finite(light.color_and_intensity) ||
      !finite(light.outer_cosine_and_enabled)) {
    return false;
  }

  const float enabled = light.outer_cosine_and_enabled[1];
  if (enabled == 0.0F) {
    const auto zero = [](const std::array<float, 4>& values) {
      for (const float value : values) {
        if (value != 0.0F) {
          return false;
        }
      }
      return true;
    };
    return zero(light.position_and_range) &&
           zero(light.direction_and_inner_cosine) &&
           zero(light.color_and_intensity) &&
           zero(light.outer_cosine_and_enabled);
  }
  if (enabled != 1.0F || !(light.position_and_range[3] > 0.0F) ||
      !(light.color_and_intensity[3] > 0.0F)) {
    return false;
  }
  for (std::size_t component = 0; component < 3; ++component) {
    if (light.color_and_intensity[component] < 0.0F) {
      return false;
    }
  }

  const float direction_length_squared =
      light.direction_and_inner_cosine[0] *
          light.direction_and_inner_cosine[0] +
      light.direction_and_inner_cosine[1] *
          light.direction_and_inner_cosine[1] +
      light.direction_and_inner_cosine[2] * light.direction_and_inner_cosine[2];
  if (std::abs(direction_length_squared - 1.0F) > 0.0001F) {
    return false;
  }
  const float inner_cosine = light.direction_and_inner_cosine[3];
  const float outer_cosine = light.outer_cosine_and_enabled[0];
  return inner_cosine >= -1.0F && inner_cosine <= 1.0F &&
         outer_cosine >= -1.0F && outer_cosine <= 1.0F &&
         inner_cosine > outer_cosine;
}

struct FrameRequest {
  FramebufferExtent framebuffer{};
  bool framebuffer_resized{};
  CameraFrame camera{};
  SpotLightFrame spot_light{};
};

[[nodiscard]] constexpr bool frameRequestCanSubmit(
    const FrameRequest& request) noexcept {
  return !request.framebuffer.isZero();
}

enum class FrameOutcome { Rendered, Skipped, Recovered };

[[nodiscard]] constexpr bool runtimeContinuesAfter(
    FrameOutcome outcome) noexcept {
  switch (outcome) {
    case FrameOutcome::Rendered:
    case FrameOutcome::Skipped:
    case FrameOutcome::Recovered:
      return true;
  }
  return false;
}

enum class LoopAction { Stop, WaitForEvents, Render };

struct LoopDecision {
  LoopAction action{LoopAction::Stop};
  FrameRequest frame{};
};

[[nodiscard]] constexpr LoopDecision decideLoopAction(
    bool close_requested, FramebufferExtent framebuffer,
    bool framebuffer_resized, CameraFrame camera = {},
    SpotLightFrame spot_light = {}) noexcept {
  if (close_requested) {
    return {};
  }
  if (framebuffer.isZero()) {
    return {LoopAction::WaitForEvents,
            {framebuffer, framebuffer_resized, camera, spot_light}};
  }
  return {LoopAction::Render,
          {framebuffer, framebuffer_resized, camera, spot_light}};
}

#endif
