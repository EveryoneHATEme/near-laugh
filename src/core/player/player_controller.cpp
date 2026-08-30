#include "core/player/player_controller.hpp"

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
constexpr float mouse_sensitivity_degrees = 0.1F;

PhysicsVector horizontalForward(float yaw_degrees) noexcept {
  const float yaw = glm::radians(yaw_degrees);
  return {std::cos(yaw), 0.0F, std::sin(yaw)};
}

glm::vec3 viewForward(float yaw_degrees, float pitch_degrees) {
  const float yaw = glm::radians(yaw_degrees);
  const float pitch = glm::radians(pitch_degrees);
  return glm::normalize(glm::vec3{std::cos(yaw) * std::cos(pitch),
                                  std::sin(pitch),
                                  std::sin(yaw) * std::cos(pitch)});
}

float horizontalLength(PhysicsVector vector) noexcept {
  return std::sqrt(vector.x * vector.x + vector.z * vector.z);
}

PhysicsVector approachHorizontal(PhysicsVector current, PhysicsVector desired,
                                 float maximum_delta) noexcept {
  const PhysicsVector difference{desired.x - current.x, 0.0F,
                                 desired.z - current.z};
  const float distance = horizontalLength(difference);
  if (distance <= maximum_delta || distance == 0.0F) {
    return desired;
  }
  const float scale = maximum_delta / distance;
  return {current.x + difference.x * scale, 0.0F,
          current.z + difference.z * scale};
}

float interpolate(float previous, float current, float alpha) noexcept {
  return previous + (current - previous) * alpha;
}
}  // namespace

PlayerController::PlayerController(PhysicsWorld& physics,
                                   float initial_yaw_degrees)
    : physics_(physics),
      state_(physics.characterState()),
      previous_presentation_{state_.foot_position, eyeHeight(state_.stance)},
      current_presentation_(previous_presentation_),
      yaw_degrees_(initial_yaw_degrees) {}

void PlayerController::sampleInput(const FpsActionSnapshot& actions,
                                   bool controls_active) {
  controls_ = controls_active ? actions : FpsActionSnapshot{};
  if (controls_active) {
    yaw_degrees_ += static_cast<float>(actions.look_delta_x) *
                    mouse_sensitivity_degrees;
    pitch_degrees_ =
        std::clamp(pitch_degrees_ -
                       static_cast<float>(actions.look_delta_y) *
                           mouse_sensitivity_degrees,
                   -player_pitch_limit_degrees, player_pitch_limit_degrees);
  }

  const bool jump_down = controls_active && actions.jump;
  if (jump_down && !jump_was_down_) {
    jump_pending_ = true;
  }
  jump_was_down_ = jump_down;
}

PhysicsVector PlayerController::desiredHorizontalVelocity() const noexcept {
  const PhysicsVector forward = horizontalForward(yaw_degrees_);
  const PhysicsVector right{-forward.z, 0.0F, forward.x};
  const float forward_axis =
      static_cast<float>(static_cast<int>(controls_.move_forward) -
                         static_cast<int>(controls_.move_backward));
  const float right_axis =
      static_cast<float>(static_cast<int>(controls_.move_right) -
                         static_cast<int>(controls_.move_left));
  PhysicsVector direction{forward.x * forward_axis + right.x * right_axis,
                          0.0F,
                          forward.z * forward_axis + right.z * right_axis};
  const float length = horizontalLength(direction);
  if (length == 0.0F) {
    return {};
  }
  direction.x /= length;
  direction.z /= length;
  const float speed = controls_.sprint ? player_sprint_speed : player_walk_speed;
  direction.x *= speed;
  direction.z *= speed;
  return direction;
}

void PlayerController::fixedStep(float delta_seconds) {
  if (!(delta_seconds > 0.0F) || !std::isfinite(delta_seconds)) {
    throw std::invalid_argument(
        "Player fixed step requires a finite positive delta");
  }

  const PhysicsVector desired = desiredHorizontalVelocity();
  if (state_.supported()) {
    requested_horizontal_velocity_ = desired;
  } else {
    requested_horizontal_velocity_ = approachHorizontal(
        {state_.linear_velocity.x, 0.0F, state_.linear_velocity.z}, desired,
        player_air_control * delta_seconds);
  }

  float vertical_velocity = state_.linear_velocity.y;
  if (state_.supported() && vertical_velocity <= 0.1F) {
    vertical_velocity = 0.0F;
  }
  if (jump_pending_ && state_.supported() &&
      state_.stance == PhysicsPlayerStance::Standing) {
    vertical_velocity = player_jump_speed;
    jump_pending_ = false;
  } else {
    vertical_velocity -= player_gravity * delta_seconds;
  }

  previous_presentation_ = current_presentation_;
  state_ = physics_.stepCharacter(
      {{requested_horizontal_velocity_.x, vertical_velocity,
        requested_horizontal_velocity_.z},
       {0.0F, -player_gravity, 0.0F}, controls_.crouch},
      delta_seconds);
  current_presentation_ = {state_.foot_position, eyeHeight(state_.stance)};
}

float PlayerController::eyeHeight(PhysicsPlayerStance stance) noexcept {
  return stance == PhysicsPlayerStance::Crouched
             ? player_crouched_eye_height
             : player_standing_eye_height;
}

void PlayerController::collapsePresentationState() noexcept {
  previous_presentation_ = current_presentation_;
}

PlayerCameraPosition PlayerController::interpolatedCameraPosition(
    float interpolation_alpha) const noexcept {
  const float alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
  return {
      interpolate(previous_presentation_.foot_position.x,
                  current_presentation_.foot_position.x, alpha),
      interpolate(previous_presentation_.foot_position.y +
                      previous_presentation_.eye_height,
                  current_presentation_.foot_position.y +
                      current_presentation_.eye_height,
                  alpha),
      interpolate(previous_presentation_.foot_position.z,
                  current_presentation_.foot_position.z, alpha),
  };
}

CameraFrame PlayerController::cameraFrame(float framebuffer_aspect,
                                          float interpolation_alpha,
                                          float recoil_pitch_degrees) const {
  if (!std::isfinite(framebuffer_aspect) || framebuffer_aspect <= 0.0F) {
    throw std::invalid_argument(
        "Player camera requires a finite positive framebuffer aspect");
  }

  const PlayerCameraPosition camera =
      interpolatedCameraPosition(interpolation_alpha);
  if (!std::isfinite(recoil_pitch_degrees)) {
    throw std::invalid_argument("Player camera recoil must be finite");
  }
  const float effective_pitch =
      std::clamp(pitch_degrees_ + recoil_pitch_degrees,
                 -player_pitch_limit_degrees, player_pitch_limit_degrees);
  const glm::vec3 position{camera.x, camera.y, camera.z};
  const glm::mat4 view = glm::lookAtRH(
      position, position + viewForward(yaw_degrees_, effective_pitch),
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

PlayerAim PlayerController::currentAim(float recoil_pitch_degrees) const {
  if (!std::isfinite(recoil_pitch_degrees)) {
    throw std::invalid_argument("Player aim recoil must be finite");
  }
  const float effective_pitch =
      std::clamp(pitch_degrees_ + recoil_pitch_degrees,
                 -player_pitch_limit_degrees, player_pitch_limit_degrees);
  const glm::vec3 direction = viewForward(yaw_degrees_, effective_pitch);
  return {{current_presentation_.foot_position.x,
           current_presentation_.foot_position.y +
               current_presentation_.eye_height,
           current_presentation_.foot_position.z},
          {direction.x, direction.y, direction.z}};
}
