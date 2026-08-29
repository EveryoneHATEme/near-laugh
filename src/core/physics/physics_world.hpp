#ifndef CORE_PHYSICS_PHYSICS_WORLD_HPP
#define CORE_PHYSICS_PHYSICS_WORLD_HPP

#include <cstddef>
#include <memory>

#include "core/world/prototype_level.hpp"

inline constexpr float player_capsule_radius = 0.35F;
inline constexpr float player_standing_height = 1.80F;
inline constexpr float player_crouched_height = 1.20F;
inline constexpr float player_maximum_step_height = 0.30F;
inline constexpr float player_maximum_slope_degrees = 50.0F;

struct PhysicsVector {
  float x{};
  float y{};
  float z{};
};

enum class PhysicsGroundState { OnGround, OnSteepGround, Unsupported, InAir };
enum class PhysicsPlayerStance { Standing, Crouched };

struct PhysicsCharacterMotion {
  PhysicsVector linear_velocity{};
  PhysicsVector gravity{0.0F, -18.0F, 0.0F};
  bool crouch_requested{};
};

struct PhysicsCharacterState {
  PhysicsVector foot_position{};
  PhysicsVector linear_velocity{};
  PhysicsGroundState ground_state{PhysicsGroundState::InAir};
  PhysicsPlayerStance stance{PhysicsPlayerStance::Standing};

  [[nodiscard]] bool supported() const noexcept {
    return ground_state == PhysicsGroundState::OnGround;
  }
};

struct PhysicsStaticSolid {
  WorldPosition center{};
  WorldExtent half_extent{};
  PrototypeSolidKind kind{PrototypeSolidKind::Obstacle};
};

class PhysicsWorld {
 public:
  explicit PhysicsWorld(const PrototypeLevel& level);
  ~PhysicsWorld();

  PhysicsWorld(const PhysicsWorld&) = delete;
  PhysicsWorld& operator=(const PhysicsWorld&) = delete;
  PhysicsWorld(PhysicsWorld&&) = delete;
  PhysicsWorld& operator=(PhysicsWorld&&) = delete;

  [[nodiscard]] PhysicsCharacterState stepCharacter(
      const PhysicsCharacterMotion& motion, float delta_seconds);
  [[nodiscard]] PhysicsCharacterState characterState() const noexcept;
  [[nodiscard]] std::size_t staticBodyCount() const noexcept;
  [[nodiscard]] PhysicsStaticSolid staticBody(std::size_t index) const;
  [[nodiscard]] bool usesSingleThreadedJobs() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

#endif
