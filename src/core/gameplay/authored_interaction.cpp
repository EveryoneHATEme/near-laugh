#include "core/gameplay/authored_interaction.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

std::optional<DoorResult> AuthoredInteraction::update(
    const PlayerActionSnapshot& input, bool active, const PlayerViewPose& view,
    const PrototypeLevel& level, const PhysicsWorld& physics,
    DoorController& doors, LightSwitchController& light_switch) {
  const std::array<bool, 3> down{input.lock, input.interact,
                                 input.secondary_action};
  std::optional<std::size_t> action;
  for (std::size_t i = 0; i < down.size(); ++i) {
    const bool press = down[i] && armed_[i];
    armed_[i] = !down[i];
    if (press && !action) action = i;
  }
  if (!action || !active) return std::nullopt;
  const WorldPosition eye{view.position.x, view.position.y, view.position.z};
  const WorldPosition direction{view.direction.x, view.direction.y,
                                view.direction.z};
  const double length =
      std::hypot(double(direction.x), double(direction.y), double(direction.z));
  if (!(length > 0) || !std::isfinite(length)) return std::nullopt;
  std::array<float, level_maximum_door_count + 1> distances;
  distances.fill(std::numeric_limits<float>::infinity());
  const auto count = level.doors().size();
  for (std::size_t i = 0; i < count; ++i) {
    const auto distance =
        doorRayDistance(level.doors()[i], doors.state(i).angle, eye, direction);
    if (distance && *distance <= 2.0F) distances[i] = *distance;
  }
  if (level.lightSwitch()) {
    const auto distance =
        lightSwitchRayDistance(*level.lightSwitch(), eye, direction);
    if (distance && *distance <= 2.0F) distances[count] = *distance;
  }
  const float nearest =
      *std::min_element(distances.begin(), distances.begin() + count + 1);
  if (!std::isfinite(nearest)) return std::nullopt;
  std::size_t chosen = count;
  for (std::size_t i = 0; i < count; ++i)
    if (distances[i] <= nearest + 0.0001F &&
        (chosen == count || level.doors()[i].id < level.doors()[chosen].id))
      chosen = i;
  const float distance = distances[chosen];
  const WorldPosition hit{eye.x + float(direction.x / length * distance),
                          eye.y + float(direction.y / length * distance),
                          eye.z + float(direction.z / length * distance)};
  if (physics.worldSegmentBlocked(
          eye, hit,
          chosen < count ? level.doors()[chosen].id : std::string_view{}))
    return std::nullopt;
  if (chosen < count)
    return doors.act(chosen,
                     *action == 0   ? DoorAction::Lock
                     : *action == 1 ? DoorAction::Interact
                                    : DoorAction::Knock,
                     eye);
  if (*action == 1) light_switch.toggle();
  return std::nullopt;
}
