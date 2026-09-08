#ifndef CORE_PHYSICS_PHYSICS_WORLD_HPP
#define CORE_PHYSICS_PHYSICS_WORLD_HPP

#include <cstddef>
#include <memory>
#include <span>

#include "core/world/prototype_level.hpp"

inline constexpr float player_capsule_radius = 0.35F;
inline constexpr float player_standing_height = 1.80F;
inline constexpr float player_crouched_height = 1.20F;
inline constexpr float player_maximum_step_height = 0.30F;
inline constexpr float player_maximum_slope_degrees =
    prototype_terrain_maximum_slope_degrees;

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
  float yaw_degrees{};
};

struct PhysicsDoorAdvance {
  float angle{};
  bool obstructed{};
};

struct PhysicsActorState {
  WorldPosition feet_position{};
  float yaw_degrees{};
};

enum class PhysicsActorObstruction {
  None,
  Static,
  Door,
  Player,
  Actor,
  Support
};

struct PhysicsActorAdvance {
  PhysicsActorState state;
  float horizontal_distance{};
  PhysicsActorObstruction obstruction{PhysicsActorObstruction::None};
};

struct PhysicsActorMotion {
  WorldPosition displacement{};
  float yaw_degrees{};
};

class PhysicsWorld {
 public:
  explicit PhysicsWorld(const PrototypeLevel& level);
  PhysicsWorld(const PrototypeLevel& level, const LevelEntry& entry);
  ~PhysicsWorld();

  PhysicsWorld(const PhysicsWorld&) = delete;
  PhysicsWorld& operator=(const PhysicsWorld&) = delete;
  PhysicsWorld(PhysicsWorld&&) = delete;
  PhysicsWorld& operator=(PhysicsWorld&&) = delete;

  // Once per fixed step, before player, actors, then doors. Moving an
  // individual participant never advances the shared world.
  void advanceWorld(float delta_seconds);
  [[nodiscard]] PhysicsCharacterState stepCharacter(
      const PhysicsCharacterMotion& motion, float delta_seconds);
  [[nodiscard]] PhysicsCharacterState characterState() const noexcept;
  [[nodiscard]] std::size_t staticBodyCount() const noexcept;
  [[nodiscard]] PhysicsStaticSolid staticBody(std::size_t index) const;
  [[nodiscard]] bool hasTerrainCollision() const noexcept;
  [[nodiscard]] bool usesSingleThreadedJobs() const noexcept;
  // Tests only the static world, including all authored prop boxes. Extends the
  // endpoint by 0.1 mm so a surface at the target counts as obstruction.
  // Invalid/zero-length segments and origins inside solid collision block.
  [[nodiscard]] bool staticSegmentBlocked(WorldPosition origin,
                                          WorldPosition endpoint) const;
  [[nodiscard]] bool worldSegmentBlocked(
      WorldPosition origin, WorldPosition endpoint,
      std::string_view selected_door = {}) const;
  [[nodiscard]] PhysicsDoorAdvance advanceDoor(std::size_t index,
                                               float requested_angle);
  [[nodiscard]] float doorAngle(std::size_t index) const;
  [[nodiscard]] std::size_t doorCount() const noexcept;
  [[nodiscard]] std::size_t actorCount() const noexcept;
  [[nodiscard]] PhysicsActorState actorState(std::size_t index) const;
  [[nodiscard]] PhysicsActorAdvance advanceActor(std::size_t index,
                                                 WorldPosition displacement,
                                                 float yaw_degrees);
  // Input/results retain authored order; acceptance runs in durable ID order.
  [[nodiscard]] std::vector<PhysicsActorAdvance> advanceActors(
      std::span<const PhysicsActorMotion> motions);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

#endif
