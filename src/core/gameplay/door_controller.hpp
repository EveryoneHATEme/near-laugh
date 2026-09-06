#ifndef CORE_GAMEPLAY_DOOR_CONTROLLER_HPP
#define CORE_GAMEPLAY_DOOR_CONTROLLER_HPP

#include <span>
#include <string_view>
#include <vector>

#include "core/physics/physics_world.hpp"
#include "core/world/door.hpp"

enum class DoorAction { Interact, Lock, Knock };
enum class DoorResultKind {
  Opening,
  Closing,
  Opened,
  Closed,
  Obstructed,
  Locked,
  Unlocked,
  Refused,
  Knocked
};
struct DoorResult {
  std::string_view id{};
  DoorResultKind kind{DoorResultKind::Closed};
};
struct DoorRuntimeState {
  float angle{};
  bool moving{};
  bool target_open{};
  bool locked{};
  DoorResultKind feedback{DoorResultKind::Closed};
  float feedback_seconds{};
  int feedback_side{1};
};

class DoorController {
 public:
  explicit DoorController(const std::vector<DoorDefinition>& definitions);
  [[nodiscard]] DoorResult act(std::size_t index, DoorAction action,
                               WorldPosition eye);
  void fixedStep(float seconds, PhysicsWorld& physics);
  [[nodiscard]] const DoorRuntimeState& state(std::size_t index) const;
  [[nodiscard]] std::span<const OpaqueBoxFrame> presentation();

 private:
  DoorResult feedback(std::size_t index, DoorResultKind kind);
  const std::vector<DoorDefinition>& definitions_;
  std::vector<DoorRuntimeState> states_;
  std::vector<std::size_t> update_order_;
  std::vector<OpaqueBoxFrame> boxes_;
};

#endif
