#include "core/gameplay/authored_interaction.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/gameplay/household_controller.hpp"
#include "core/world/household.hpp"

std::optional<AuthoredTarget> selectAuthoredTarget(
    const PlayerViewPose& view, const PrototypeLevel& level,
    const PhysicsWorld& physics, const DoorController& doors) {
  const WorldPosition eye{view.position.x, view.position.y, view.position.z};
  const WorldPosition direction{view.direction.x, view.direction.y,
                                view.direction.z};
  const double length =
      std::hypot(double(direction.x), double(direction.y), double(direction.z));
  if (!(length > 0) || !std::isfinite(length)) return std::nullopt;
  struct Candidate {
    AuthoredTarget target;
    std::string_view id;
  };
  std::vector<Candidate> candidates;
  candidates.reserve(level.doors().size() + level.lightSwitches().size() +
                     level.household().boxes.size() +
                     level.household().documents.size() +
                     level.household().radios.size());
  const auto add = [&](AuthoredTargetKind kind, std::size_t index,
                       std::string_view id, std::optional<float> distance) {
    if (distance && *distance <= 2.F)
      candidates.push_back({{kind, index, *distance}, id});
  };
  for (std::size_t i = 0; i < level.doors().size(); ++i)
    add(AuthoredTargetKind::Door, i, level.doors()[i].id,
        doorRayDistance(level.doors()[i], doors.state(i).angle, eye,
                        direction));
  for (std::size_t i = 0; i < level.lightSwitches().size(); ++i) {
    const auto& definition = level.lightSwitches()[i];
    if (lightSwitchPointInside(definition, eye)) return std::nullopt;
    add(AuthoredTargetKind::LightSwitch, i, definition.id,
        lightSwitchRayDistance(definition, eye, direction));
  }
  for (std::size_t i = 0; i < level.household().boxes.size(); ++i) {
    const auto state = physics.boxState(i);
    const auto bounds =
        householdBoxPresentation(state.center, state.orientation);
    if (householdPointInside(bounds, eye)) return std::nullopt;
    if (physics.heldBox() != i)
      add(AuthoredTargetKind::Box, i, level.household().boxes[i].id,
          householdRayDistance(bounds, eye, direction));
  }
  for (std::size_t i = 0; i < level.household().documents.size(); ++i) {
    const auto& definition = level.household().documents[i];
    const auto bounds = householdDocumentPresentation(definition);
    if (householdPointInside(bounds, eye)) return std::nullopt;
    add(AuthoredTargetKind::Document, i, definition.id,
        householdRayDistance(bounds, eye, direction));
  }
  for (std::size_t i = 0; i < level.household().radios.size(); ++i) {
    const auto& definition = level.household().radios[i];
    const auto prop =
        std::find_if(level.props().begin(), level.props().end(),
                     [&](const auto& p) { return p.id == definition.prop; });
    if (prop == level.props().end()) continue;
    const auto bounds = householdRadioBounds(*prop);
    if (householdPointInside(bounds, eye)) return std::nullopt;
    add(AuthoredTargetKind::Radio, i, definition.id,
        householdRayDistance(bounds, eye, direction));
  }
  if (candidates.empty()) return std::nullopt;
  const float nearest =
      std::min_element(candidates.begin(), candidates.end(),
                       [](const auto& a, const auto& b) {
                         return a.target.distance < b.target.distance;
                       })
          ->target.distance;
  const Candidate* chosen = nullptr;
  for (const auto& candidate : candidates) {
    if (candidate.target.distance > nearest + .0001F) continue;
    if (!chosen || candidate.target.kind < chosen->target.kind ||
        (candidate.target.kind == chosen->target.kind &&
         candidate.id < chosen->id))
      chosen = &candidate;
  }
  const auto& target = chosen->target;
  const WorldPosition hit{
      float(eye.x + direction.x / length * target.distance),
      float(eye.y + direction.y / length * target.distance),
      float(eye.z + direction.z / length * target.distance)};
  if (physics.worldSegmentBlocked(
          eye, hit,
          target.kind == AuthoredTargetKind::Door ? chosen->id
                                                  : std::string_view{},
          target.kind == AuthoredTargetKind::Box ? chosen->id
                                                 : std::string_view{}))
    return std::nullopt;
  return target;
}

std::optional<DoorResult> AuthoredInteraction::update(
    const PlayerActionSnapshot& input, bool active, const PlayerViewPose& view,
    const PrototypeLevel& level, const PhysicsWorld& physics,
    DoorController& doors, LightSwitchController& light_switch,
    HouseholdController* household) {
  const std::array down{input.lock, input.interact, input.secondary_action};
  std::array<bool, 3> press{};
  for (std::size_t i = 0; i < down.size(); ++i) {
    press[i] = down[i] && armed_[i];
    armed_[i] = !down[i];
  }
  if (household) household->hint({});
  if (!active || (household && !household->worldActionsAllowed()))
    return std::nullopt;
  if (household && household->heldBox()) {
    if (press[1])
      (void)household->requestDrop();
    else if (press[2])
      (void)household->requestThrow(view.direction);
    else if (press[0])
      household->feedback(HouseholdResult::Refused, "Сначала положите коробку");
    return std::nullopt;
  }
  std::optional<std::size_t> action;
  for (std::size_t i = 0; i < press.size(); ++i)
    if (press[i] && !action) action = i;
  if (!action && !household) return std::nullopt;
  if (action && household && household->commandPending()) {
    household->feedback(HouseholdResult::Busy,
                        "Действие уже ожидает выполнения");
    return std::nullopt;
  }
  const auto target = selectAuthoredTarget(view, level, physics, doors);
  if (!target) return std::nullopt;
  if (household) {
    switch (target->kind) {
      case AuthoredTargetKind::Door:
        household->hint("E — дверь; R — замок; правая кнопка — постучать");
        break;
      case AuthoredTargetKind::LightSwitch:
        household->hint("E — переключить свет");
        break;
      case AuthoredTargetKind::Box:
        household->hint("E — поднять коробку");
        break;
      case AuthoredTargetKind::Document:
        household->hint("E — читать документ");
        break;
      case AuthoredTargetKind::Radio:
        household->hint(household->radioOn(target->index)
                            ? "E — выключить радио"
                            : "E — включить радио");
        break;
    }
  }
  if (!action) return std::nullopt;
  const WorldPosition eye{view.position.x, view.position.y, view.position.z};
  if (target->kind == AuthoredTargetKind::Door)
    return doors.act(target->index,
                     *action == 0   ? DoorAction::Lock
                     : *action == 1 ? DoorAction::Interact
                                    : DoorAction::Knock,
                     eye);
  if (*action != 1) return std::nullopt;
  if (target->kind == AuthoredTargetKind::LightSwitch)
    light_switch.toggle(target->index);
  else if (household) {
    switch (target->kind) {
      case AuthoredTargetKind::Box:
        (void)household->requestPickup(target->index, view);
        break;
      case AuthoredTargetKind::Document:
        household->openDocument(target->index);
        break;
      case AuthoredTargetKind::Radio:
        household->toggleRadio(target->index);
        break;
      default:
        break;
    }
  }
  return std::nullopt;
}
