#include "core/gameplay/door_controller.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

DoorController::DoorController(const std::vector<DoorDefinition>& definitions)
    : definitions_(definitions),
      states_(definitions.size()),
      update_order_(definitions.size()),
      boxes_(definitions.size() * 6) {
  if (definitions.size() > level_maximum_door_count)
    throw std::invalid_argument("Door count exceeds runtime bound");
  std::iota(update_order_.begin(), update_order_.end(), 0);
  std::sort(update_order_.begin(), update_order_.end(), [&](auto a, auto b) {
    return definitions[a].id < definitions[b].id;
  });
  for (std::size_t i = 0; i < definitions.size(); ++i) {
    states_[i].angle = doorInitialAngle(definitions[i]);
    states_[i].target_open = definitions[i].initially_open;
    states_[i].locked = definitions[i].initially_locked;
  }
}

DoorResult DoorController::feedback(std::size_t index, DoorResultKind kind) {
  auto& state = states_[index];
  state.feedback = kind;
  state.feedback_seconds = 0.3F;
  return {definitions_[index].id, kind};
}

DoorResult DoorController::act(std::size_t index, DoorAction action,
                               WorldPosition eye) {
  auto& state = states_.at(index);
  const auto& door = definitions_[index];
  const auto local = doorLocalPoint(door, state.angle, eye);
  state.feedback_side = local.z < 0 ? -1 : 1;
  if (action == DoorAction::Knock)
    return feedback(index, DoorResultKind::Knocked);
  if (action == DoorAction::Lock) {
    const auto closed_local = doorLocalPoint(door, 0, eye);
    const bool side =
        (door.lock_side == DoorLockSide::PositiveZ &&
         closed_local.z > 0.001F) ||
        (door.lock_side == DoorLockSide::NegativeZ && closed_local.z < -0.001F);
    if (!side || state.moving || state.angle != 0)
      return feedback(index, DoorResultKind::Refused);
    state.locked = !state.locked;
    return feedback(index, state.locked ? DoorResultKind::Locked
                                        : DoorResultKind::Unlocked);
  }
  const bool open = state.angle == 0 ? true
                    : state.angle == door.open_angle_degrees
                        ? false
                        : !state.target_open;
  if (open && state.locked) return feedback(index, DoorResultKind::Refused);
  state.target_open = open;
  state.moving = true;
  return feedback(index,
                  open ? DoorResultKind::Opening : DoorResultKind::Closing);
}

void DoorController::fixedStep(float seconds, PhysicsWorld& physics) {
  if (!(seconds > 0) || !std::isfinite(seconds))
    throw std::invalid_argument("Door step requires finite positive time");
  for (const auto i : update_order_) {
    auto& state = states_[i];
    const auto& door = definitions_[i];
    state.feedback_seconds = std::max(0.0F, state.feedback_seconds - seconds);
    if (!state.moving) continue;
    const float endpoint = state.target_open ? door.open_angle_degrees : 0;
    const float distance = endpoint - state.angle;
    const float step =
        std::min(std::abs(distance), door.speed_degrees_per_second * seconds);
    const float requested = step == std::abs(distance)
                                ? endpoint
                                : state.angle + std::copysign(step, distance);
    const auto result = physics.advanceDoor(i, requested);
    state.angle = result.angle;
    if (result.obstructed) {
      state.moving = false;
      (void)feedback(i, DoorResultKind::Obstructed);
    } else if (state.angle == endpoint) {
      state.moving = false;
      (void)feedback(i, state.target_open ? DoorResultKind::Opened
                                          : DoorResultKind::Closed);
    }
  }
}

const DoorRuntimeState& DoorController::state(std::size_t index) const {
  return states_.at(index);
}

std::span<const OpaqueBoxFrame> DoorController::presentation() {
  for (std::size_t i = 0; i < definitions_.size(); ++i) {
    const auto& state = states_[i];
    const float pulse =
        state.feedback_seconds > 0
            ? std::sin(state.feedback_seconds / 0.3F * 3.14159265F)
            : 0;
    const auto boxes = doorPresentationBoxes(
        definitions_[i], state.angle, state.locked,
        state.feedback == DoorResultKind::Refused ? pulse : 0,
        state.feedback == DoorResultKind::Knocked ? std::max(0.0F, pulse) : 0,
        state.feedback_side);
    std::copy(boxes.begin(), boxes.end(), boxes_.begin() + i * 6);
  }
  return boxes_;
}
