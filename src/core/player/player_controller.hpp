#ifndef CORE_PLAYER_PLAYER_CONTROLLER_HPP
#define CORE_PLAYER_PLAYER_CONTROLLER_HPP

#include "core/frame.hpp"
#include "core/input/fps_input.hpp"
#include "core/physics/physics_world.hpp"

inline constexpr float player_standing_eye_height = 1.65F;
inline constexpr float player_crouched_eye_height = 1.05F;
inline constexpr float player_walk_speed = 4.0F;
inline constexpr float player_sprint_speed = 7.0F;
inline constexpr float player_gravity = 18.0F;
inline constexpr float player_jump_speed = 6.5F;
inline constexpr float player_air_control = 8.0F;
inline constexpr float player_pitch_limit_degrees = 89.0F;

struct PlayerCameraPosition {
  float x{};
  float y{};
  float z{};
};

struct PlayerAim {
  PhysicsVector eye_position{};
  PhysicsVector direction{};
};

struct PlayerPresentationState {
  PhysicsVector foot_position{};
  float eye_height{player_standing_eye_height};
};

enum class PlayerCursorCaptureTransition { None, Release, Capture };

[[nodiscard]] constexpr PlayerCursorCaptureTransition playerCursorTransition(
    bool cursor_captured, const FpsActionSnapshot& actions) noexcept {
  if (actions.menu) {
    return cursor_captured ? PlayerCursorCaptureTransition::Release
                           : PlayerCursorCaptureTransition::None;
  }
  if (!cursor_captured && actions.primary_action) {
    return PlayerCursorCaptureTransition::Capture;
  }
  return PlayerCursorCaptureTransition::None;
}

[[nodiscard]] constexpr bool playerControlsActive(
    bool cursor_captured, PlayerCursorCaptureTransition transition) noexcept {
  return cursor_captured && transition == PlayerCursorCaptureTransition::None;
}

class PlayerController {
 public:
  explicit PlayerController(PhysicsWorld& physics,
                            float initial_yaw_degrees = -90.0F);

  PlayerController(const PlayerController&) = delete;
  PlayerController& operator=(const PlayerController&) = delete;
  PlayerController(PlayerController&&) = delete;
  PlayerController& operator=(PlayerController&&) = delete;

  void sampleInput(const FpsActionSnapshot& actions, bool controls_active);
  void fixedStep(float delta_seconds);
  void collapsePresentationState() noexcept;

  [[nodiscard]] CameraFrame cameraFrame(float framebuffer_aspect,
                                        float interpolation_alpha,
                                        float recoil_pitch_degrees = 0.0F) const;
  [[nodiscard]] PlayerAim currentAim(
      float recoil_pitch_degrees = 0.0F) const;
  [[nodiscard]] PlayerCameraPosition interpolatedCameraPosition(
      float interpolation_alpha) const noexcept;
  [[nodiscard]] const PhysicsCharacterState& state() const noexcept {
    return state_;
  }
  [[nodiscard]] PhysicsVector requestedHorizontalVelocity() const noexcept {
    return requested_horizontal_velocity_;
  }
  [[nodiscard]] float yawDegrees() const noexcept { return yaw_degrees_; }
  [[nodiscard]] float pitchDegrees() const noexcept { return pitch_degrees_; }
  [[nodiscard]] bool jumpPending() const noexcept { return jump_pending_; }
  [[nodiscard]] const PlayerPresentationState& previousPresentation() const
      noexcept {
    return previous_presentation_;
  }
  [[nodiscard]] const PlayerPresentationState& currentPresentation() const
      noexcept {
    return current_presentation_;
  }

 private:
  [[nodiscard]] PhysicsVector desiredHorizontalVelocity() const noexcept;
  [[nodiscard]] static float eyeHeight(PhysicsPlayerStance stance) noexcept;

  PhysicsWorld& physics_;
  FpsActionSnapshot controls_{};
  PhysicsCharacterState state_{};
  PhysicsVector requested_horizontal_velocity_{};
  PlayerPresentationState previous_presentation_{};
  PlayerPresentationState current_presentation_{};
  float yaw_degrees_{};
  float pitch_degrees_{};
  bool jump_was_down_{};
  bool jump_pending_{};
};

#endif
