#include "core/gameplay/character_controller.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <utility>

#include "core/development/frame_timings.hpp"

namespace {
constexpr float arrival_distance = .02F;
float yawDelta(float from, float to) {
  float delta = std::remainder(
      std::remainder(to, 360.F) - std::remainder(from, 360.F), 360.F);
  if (delta == -180) delta = 180;
  return delta;
}
float turn(float from, float to, float seconds) {
  return std::remainder(
      from + std::clamp(yawDelta(from, to), -120 * seconds, 120 * seconds),
      360.F);
}
float distance(WorldPosition a, WorldPosition b) {
  return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}
}  // namespace

CharacterController::Actor::Actor(
    const CharacterCatalogEntry* selected,
    std::shared_ptr<const CharacterAsset> prepared)
    : profile(selected), asset(std::move(prepared)), playback(asset) {}

CharacterController::CharacterController(
    const LevelCharacters& definitions,
    std::span<const std::shared_ptr<const CharacterAsset>> assets,
    PhysicsWorld& physics, CueCoordinator& audio)
    : definitions_(definitions), physics_(physics), audio_(audio) {
  if (assets.size() != definitions.actors.size() ||
      assets.size() != physics.actorCount())
    throw std::invalid_argument("Character preparation/physics count mismatch");
  frames_.resize(assets.size());
  actors_.reserve(assets.size());
  order_.resize(assets.size());
  std::iota(order_.begin(), order_.end(), 0);
  std::sort(order_.begin(), order_.end(), [&](auto a, auto b) {
    return definitions.actors[a].id < definitions.actors[b].id;
  });
  for (std::size_t i = 0; i < assets.size(); ++i) {
    const auto& definition = definitions.actors[i];
    const auto* profile = findCharacterModel(definition.model);
    if (!profile)
      throw std::invalid_argument("Character model was not resolved");
    actors_.emplace_back(profile, assets[i]);
    actors_.back().result.actor = definition.id;
    publish(i);
  }
  for (auto i : order_)
    if (const auto& route = definitions.actors[i].initial_route)
      (void)start(definitions.actors[i].id, *route);
}

std::size_t CharacterController::actorIndex(std::string_view id) const {
  for (std::size_t i = 0; i < actors_.size(); ++i)
    if (definitions_.actors[i].id == id) return i;
  throw std::invalid_argument("Unknown actor '" + std::string(id) + "'");
}
CharacterStart CharacterController::start(std::string_view actor_id,
                                          std::string_view route_id) {
  const auto i = actorIndex(actor_id);
  auto& actor = actors_[i];
  const auto* route = findCharacterRoute(definitions_, route_id);
  if (!route || route->actor != actor_id)
    throw std::invalid_argument("Route does not belong to actor '" +
                                std::string(actor_id) + "'");
  if (actor.stage != Stage::Inactive)
    return actor.route == route ? CharacterStart::AlreadyActive
                                : CharacterStart::Busy;
  cancelSounds(i);
  std::erase_if(contacts_,
                [i](const Contact& contact) { return contact.actor == i; });
  const auto serial = actor.result.instance + 1;
  actor.result = {definitions_.actors[i].id, route->id, route->marks.front(),
                  serial, CharacterAction::Turning};
  actor.route = route;
  actor.mark = 0;
  actor.stage = Stage::Heading;
  actor.interaction_fired = actor.pending_interaction = false;
  actor.playback.selectClip("idle");
  return CharacterStart::Started;
}
void CharacterController::cancelSounds(std::size_t i) {
  auto& actor = actors_[i];
  const auto stop = [&](const auto& source, std::uint64_t instance) {
    if (source && instance && audio_.instance(*source) == instance)
      (void)audio_.cancel(*source);
  };
  stop(definitions_.actors[i].footstep_source, actor.footstep_instance);
  stop(definitions_.actors[i].interaction_source, actor.interaction_instance);
  actor.footstep_instance = actor.interaction_instance = 0;
}
bool CharacterController::cancel(std::string_view id) {
  const auto i = actorIndex(id);
  auto& actor = actors_[i];
  if (actor.result.action == CharacterAction::Idle ||
      actor.result.action == CharacterAction::Canceled)
    return false;
  cancelSounds(i);
  std::erase_if(contacts_,
                [i](const Contact& contact) { return contact.actor == i; });
  actor.pending_interaction = false;
  actor.stage = Stage::Inactive;
  actor.result.action = CharacterAction::Canceled;
  actor.result.obstruction = PhysicsActorObstruction::None;
  actor.result.audio_contention = false;
  actor.playback.selectClip("idle");
  publish(i);
  return true;
}
void CharacterController::nextMark(Actor& actor) {
  if (++actor.mark < actor.route->marks.size()) {
    actor.result.mark = actor.route->marks[actor.mark];
    actor.stage = Stage::Heading;
    actor.result.action = CharacterAction::Turning;
  } else if (actor.route->final_clip) {
    actor.stage = Stage::Interaction;
    actor.result.action = CharacterAction::Interacting;
    actor.playback.selectClip("interact");
  } else {
    actor.stage = Stage::Inactive;
    actor.result.action = CharacterAction::Completed;
    actor.playback.selectClip("idle");
  }
}
void CharacterController::fixedStep(float seconds, FrameTimingSample* timings) {
  using Clock = std::chrono::steady_clock;
  auto scope_start = timings ? Clock::now() : Clock::time_point{};
  const auto finish_scope = [&](double FrameTimingSample::* field) {
    if (!timings) return;
    const auto now = Clock::now();
    timings->*field +=
        std::chrono::duration<double, std::milli>(now - scope_start).count();
    scope_start = now;
  };
  if (!std::isfinite(seconds) || seconds <= 0 || seconds > .1F)
    throw std::invalid_argument(
        "Character step must be within (0, 0.1] seconds");
  std::vector<PhysicsActorMotion> proposals(actors_.size());
  for (auto i : order_) {
    auto& actor = actors_[i];
    const auto pose = physics_.actorState(i);
    auto& proposal = proposals[i];
    proposal.yaw_degrees = pose.yaw_degrees;
    if (actor.stage == Stage::Inactive || actor.stage == Stage::Interaction)
      continue;
    const auto& mark = *findCharacterMark(definitions_, actor.result.mark);
    const float dx = mark.feet_position.x - pose.feet_position.x;
    const float dz = mark.feet_position.z - pose.feet_position.z;
    const float horizontal = std::hypot(dx, dz);
    actor.result.obstruction = PhysicsActorObstruction::None;
    if (distance(pose.feet_position, mark.feet_position) <= arrival_distance)
      actor.stage = Stage::Facing;
    if (actor.stage == Stage::Heading) {
      if (horizontal < .0001F)
        actor.stage = Stage::Travel;
      else {
        const float heading =
            std::atan2(dx, dz) * 180 / std::numbers::pi_v<float>;
        proposal.yaw_degrees = turn(pose.yaw_degrees, heading, seconds);
        if (std::abs(yawDelta(proposal.yaw_degrees, heading)) <= 1)
          actor.stage = Stage::Travel;
        actor.result.action = CharacterAction::Turning;
        continue;  // Standing turn; no travel in the same step.
      }
    }
    if (actor.stage == Stage::Facing) {
      actor.result.action = CharacterAction::Turning;
      proposal.yaw_degrees = turn(pose.yaw_degrees, mark.yaw_degrees, seconds);
    } else if (horizontal < .0001F) {
      actor.result.action = CharacterAction::Blocked;
      actor.result.obstruction = PhysicsActorObstruction::Support;
    } else {
      const float travel =
          std::min(horizontal, definitions_.actors[i].speed * seconds);
      proposal.displacement = {dx / horizontal * travel, 0,
                               dz / horizontal * travel};
    }
  }
  finish_scope(&FrameTimingSample::route_decision_ms);
  const auto accepted = physics_.advanceActors(proposals);
  finish_scope(&FrameTimingSample::actor_physics_ms);
  for (auto i : order_) {
    auto& actor = actors_[i];
    const auto& motion = accepted[i];
    const float requested =
        std::hypot(proposals[i].displacement.x, proposals[i].displacement.z);
    if (requested > 0) {
      actor.result.obstruction = motion.obstruction;
      actor.result.action = motion.obstruction == PhysicsActorObstruction::None
                                ? CharacterAction::Walking
                                : CharacterAction::Blocked;
      if (motion.horizontal_distance > .000001F) {
        const double before =
            actor.result.walked_distance / actor.profile->walk_cycle_distance_m;
        actor.result.walked_distance += motion.horizontal_distance;
        const double after =
            actor.result.walked_distance / actor.profile->walk_cycle_distance_m;
        for (const auto phase : actor.profile->walk_contact_phases) {
          for (double crossing = std::floor(before - phase) + 1 + phase;
               crossing <= after; crossing += 1) {
            contacts_.push_back({i, actor.result.instance});
            ++actor.result.contacts;
          }
        }
        actor.playback.selectClip("walk");
        actor.playback.drive(
            after * characterClip(*actor.asset, "walk").duration, seconds);
      } else {
        actor.playback.selectClip("idle");
        (void)actor.playback.advance(seconds);
      }
    } else if (actor.stage == Stage::Interaction) {
      const auto duration = characterClip(*actor.asset, "interact").duration;
      const auto marker = duration * actor.profile->interaction_phase;
      if (!actor.interaction_fired) {
        actor.playback.drive(std::min(marker, actor.playback.time() + seconds),
                             seconds);
        if (actor.playback.time() >= marker) {
          if (definitions_.actors[i].interaction_source)
            actor.pending_interaction = true;
          else
            actor.interaction_fired = true;
        }
      } else if (actor.playback.advance(seconds)) {
        actor.stage = Stage::Inactive;
        actor.result.action = CharacterAction::Completed;
        actor.playback.selectClip("idle");
      }
    } else {
      actor.playback.selectClip("idle");
      (void)actor.playback.advance(seconds);
      if (actor.stage == Stage::Facing) {
        const auto& mark = *findCharacterMark(definitions_, actor.result.mark);
        if (distance(motion.state.feet_position, mark.feet_position) <=
                arrival_distance &&
            std::abs(yawDelta(motion.state.yaw_degrees, mark.yaw_degrees)) <= 1)
          nextMark(actor);
      }
    }
    publish(i);
  }
  finish_scope(&FrameTimingSample::character_pose_ms);
}
void CharacterController::handoffAudio() {
  for (auto i : order_) {
    const auto feet = physics_.actorState(i).feet_position;
    for (const auto* source : {&definitions_.actors[i].footstep_source,
                               &definitions_.actors[i].interaction_source})
      if (*source) audio_.moveSource(**source, feet);
  }
  for (const auto contact : contacts_) {
    auto& actor = actors_[contact.actor];
    const auto& source = definitions_.actors[contact.actor].footstep_source;
    if (actor.result.instance == contact.action && source &&
        audio_.start(*source) == CueStart::Started)
      actor.footstep_instance = audio_.instance(*source);
  }
  contacts_.clear();
  for (auto i : order_) {
    auto& actor = actors_[i];
    if (!actor.pending_interaction) continue;
    actor.pending_interaction = false;
    const auto& source = *definitions_.actors[i].interaction_source;
    const auto started = audio_.start(source);
    actor.result.audio_contention = started == CueStart::Busy;
    if (started == CueStart::Started) {
      actor.interaction_fired = true;
      actor.interaction_instance = audio_.instance(source);
    }
  }
}
void CharacterController::publish(std::size_t i) {
  auto& actor = actors_[i];
  actor.palette = actor.playback.pose();
  const auto accepted = physics_.actorState(i);
  frames_[i] = {static_cast<std::uint32_t>(i),
                actor.asset->skeleton_identity,
                actor.palette,
                {accepted.feet_position.x, accepted.feet_position.y,
                 accepted.feet_position.z},
                accepted.yaw_degrees};
}
const CharacterActionResult& CharacterController::result(
    std::size_t actor) const {
  return actors_.at(actor).result;
}
const CharacterPlayback& CharacterController::playback(
    std::size_t actor) const {
  return actors_.at(actor).playback;
}
std::vector<CharacterRenderInstance> CharacterController::renderInstances()
    const {
  std::vector<CharacterRenderInstance> instances;
  for (std::size_t i = 0; i < actors_.size(); ++i)
    instances.push_back({static_cast<std::uint32_t>(i), actors_[i].asset});
  return instances;
}
bool CharacterController::ownsSource(std::string_view source) const noexcept {
  for (const auto& actor : definitions_.actors)
    if ((actor.footstep_source && *actor.footstep_source == source) ||
        (actor.interaction_source && *actor.interaction_source == source))
      return true;
  return false;
}
