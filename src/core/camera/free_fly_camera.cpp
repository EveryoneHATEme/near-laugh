#include "core/camera/free_fly_camera.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <stdexcept>

namespace {
static_assert(sizeof(glm::mat4) == sizeof(CameraFrame));

constexpr float vertical_field_of_view_degrees = 75.0F;
constexpr float near_plane = 0.1F;
constexpr float far_plane = 100.0F;
constexpr float movement_speed = 4.0F;
constexpr float sprint_multiplier = 3.0F;
constexpr float mouse_sensitivity_degrees = 0.1F;
constexpr float pitch_limit_degrees = 89.0F;

glm::vec3 horizontalForward(float yaw_degrees) {
  const float yaw = glm::radians(yaw_degrees);
  return glm::normalize(glm::vec3{std::cos(yaw), 0.0F, std::sin(yaw)});
}

glm::vec3 viewForward(float yaw_degrees, float pitch_degrees) {
  const float yaw = glm::radians(yaw_degrees);
  const float pitch = glm::radians(pitch_degrees);
  return glm::normalize(glm::vec3{std::cos(yaw) * std::cos(pitch),
                                  std::sin(pitch),
                                  std::sin(yaw) * std::cos(pitch)});
}
}  // namespace

void FreeFlyCamera::update(const FpsActionSnapshot& actions,
                           double elapsed_seconds) {
  yaw_degrees_ +=
      static_cast<float>(actions.look_delta_x) * mouse_sensitivity_degrees;
  pitch_degrees_ =
      std::clamp(pitch_degrees_ - static_cast<float>(actions.look_delta_y) *
                                      mouse_sensitivity_degrees,
                 -pitch_limit_degrees, pitch_limit_degrees);

  const glm::vec3 forward = horizontalForward(yaw_degrees_);
  const glm::vec3 right =
      glm::normalize(glm::cross(forward, glm::vec3{0, 1, 0}));
  glm::vec3 direction{};
  direction +=
      forward * static_cast<float>(static_cast<int>(actions.move_forward) -
                                   static_cast<int>(actions.move_backward));
  direction += right * static_cast<float>(static_cast<int>(actions.move_right) -
                                          static_cast<int>(actions.move_left));
  direction.y += static_cast<float>(static_cast<int>(actions.jump) -
                                    static_cast<int>(actions.crouch));
  const float length = glm::length(direction);
  if (length <= 0.0F) {
    return;
  }

  direction /= length;
  const float speed =
      movement_speed * (actions.sprint ? sprint_multiplier : 1.0F);
  const float distance =
      speed * static_cast<float>(boundedElapsedSeconds(elapsed_seconds));
  const glm::vec3 position{position_.x, position_.y, position_.z};
  const glm::vec3 moved = position + direction * distance;
  position_ = {moved.x, moved.y, moved.z};
}

CameraFrame FreeFlyCamera::frame(float framebuffer_aspect) const {
  if (!std::isfinite(framebuffer_aspect) || framebuffer_aspect <= 0.0F) {
    throw std::invalid_argument(
        "Free-fly camera requires a finite positive framebuffer aspect");
  }

  const glm::vec3 position{position_.x, position_.y, position_.z};
  const glm::mat4 view = glm::lookAtRH(
      position, position + viewForward(yaw_degrees_, pitch_degrees_),
      glm::vec3{0.0F, 1.0F, 0.0F});
  glm::mat4 projection =
      glm::perspectiveRH_ZO(glm::radians(vertical_field_of_view_degrees),
                            framebuffer_aspect, near_plane, far_plane);
  projection[1][1] *= -1.0F;
  const glm::mat4 view_projection = projection * view;

  CameraFrame result;
  std::memcpy(result.view_projection.data(), glm::value_ptr(view_projection),
              sizeof(view_projection));
  return result;
}

double FrameClock::sample(Clock::time_point now) noexcept {
  if (!previous_) {
    previous_ = now;
    return 0.0;
  }
  const double elapsed =
      std::chrono::duration<double>(now - *previous_).count();
  previous_ = now;
  return boundedElapsedSeconds(elapsed);
}
