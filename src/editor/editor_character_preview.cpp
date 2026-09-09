#include "editor/editor_character_preview.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "core/world/characters.hpp"

namespace {
CharacterActorDefinition selectedActor(
    const EditorDocument& document,
    const EditorCharacterPreviewRequest& request) {
  const auto value = document.object(request.actor);
  if (!value || !std::holds_alternative<CharacterActorDefinition>(*value))
    throw std::runtime_error("Choose an actor for clip inspection.");
  return std::get<CharacterActorDefinition>(*value);
}
CharacterMarkDefinition selectedMark(const EditorDocument& document,
                                     const CharacterActorDefinition& actor,
                                     EditorObjectId id) {
  if (id) {
    const auto value = document.object(id);
    if (!value || !std::holds_alternative<CharacterMarkDefinition>(*value))
      throw std::runtime_error("The chosen scene mark is missing.");
    return std::get<CharacterMarkDefinition>(*value);
  }
  const auto* mark =
      findCharacterMark(document.document()->characters, actor.initial_mark);
  if (!mark)
    throw std::runtime_error("Actor '" + actor.id +
                             "': missing initial mark '" + actor.initial_mark +
                             "'.");
  return *mark;
}
double yawDelta(double from, double to) {
  const double delta = std::remainder(
      std::remainder(to, 360.) - std::remainder(from, 360.), 360.);
  return delta == -180 ? 180 : delta;
}
CharacterPlacement placement(const CharacterMarkDefinition& mark) {
  return {{mark.feet_position.x, mark.feet_position.y, mark.feet_position.z},
          mark.yaw_degrees};
}
}  // namespace

std::string EditorCharacterPreview::startError(
    const EditorDocument& document,
    const EditorCharacterPreviewRequest& request) {
  try {
    if (!document.document())
      throw std::runtime_error("Open a level to inspect characters.");
    if (request.mode == EditorCharacterPreviewMode::Route) {
      const auto value = document.object(document.selection());
      if (!value || !std::holds_alternative<CharacterRouteDefinition>(*value))
        throw std::runtime_error("Select a route for schematic inspection.");
      const auto& route = std::get<CharacterRouteDefinition>(*value);
      const auto owner = document.object(request.actor);
      if (!owner || !std::holds_alternative<CharacterActorDefinition>(*owner) ||
          std::get<CharacterActorDefinition>(*owner).id != route.actor)
        throw std::runtime_error("Route '" + route.id +
                                 "': unresolved owner '" + route.actor + "'.");
    }
    const auto actor = selectedActor(document, request);
    const auto* model = findCharacterModel(actor.model);
    if (!model)
      throw std::runtime_error("Actor '" + actor.id + "': unknown model '" +
                               actor.model + "'.");
    if (!std::isfinite(actor.speed) || actor.speed < model->minimum_speed_m_s ||
        actor.speed > model->maximum_speed_m_s)
      throw std::runtime_error("Actor '" + actor.id + "': unsupported speed.");
    // Even at an explicit inspection mark, the initial actor must be
    // resolvable.
    const auto initial = selectedMark(document, actor, 0);
    const auto mark = selectedMark(document, actor, request.mark);
    for (const auto& value : {initial, mark})
      if (const auto error = editorCharacterFieldError(value); !error.empty())
        throw std::runtime_error("Mark '" + value.id + "': " + error);
    if (request.mode == EditorCharacterPreviewMode::Clip) {
      if (request.clip != "idle" && request.clip != "walk" &&
          request.clip != "interact")
        throw std::runtime_error("Unsupported clip '" + request.clip + "'.");
    } else {
      const auto selected = document.object(document.selection());
      if (!selected ||
          !std::holds_alternative<CharacterRouteDefinition>(*selected))
        throw std::runtime_error("Select a route for schematic inspection.");
      const auto& route = std::get<CharacterRouteDefinition>(*selected);
      if (route.actor != actor.id)
        throw std::runtime_error("Route '" + route.id +
                                 "': unresolved owner '" + route.actor + "'.");
      if (route.marks.empty() ||
          route.marks.size() > level_maximum_route_mark_count)
        throw std::runtime_error("Route '" + route.id +
                                 "': invalid mark count.");
      if (route.final_clip && route.final_clip != "interact")
        throw std::runtime_error("Route '" + route.id +
                                 "': unsupported final clip '" +
                                 *route.final_clip + "'.");
      for (const auto& id : route.marks) {
        const auto* endpoint =
            findCharacterMark(document.document()->characters, id);
        if (!endpoint)
          throw std::runtime_error("Route '" + route.id + "': missing mark '" +
                                   id + "'.");
        if (const auto error = editorCharacterFieldError(*endpoint);
            !error.empty())
          throw std::runtime_error("Mark '" + id + "': " + error);
      }
    }
    return {};
  } catch (const std::exception& error) {
    return error.what();
  }
}

bool EditorCharacterPreview::start(
    const EditorDocument& document,
    const EditorCharacterPreviewRequest& request,
    std::shared_ptr<const CharacterAsset> asset) {
  stop();
  generation_ = document.generation();
  revision_ = document.revision();
  selection_ = document.selection();
  selection_revision_ = document.selectionRevision();
  error_ = startError(document, request);
  if (!error_.empty()) return false;
  try {
    if (!asset)
      throw std::runtime_error("Current character resources are unavailable.");
    for (const auto id : {"idle", "walk", "interact"})
      (void)characterClip(*asset, id);
    const auto actor = selectedActor(document, request);
    mode_ = request.mode;
    asset_ = std::move(asset);
    playback_.emplace(asset_, mode_ == EditorCharacterPreviewMode::Clip
                                  ? request.clip
                                  : "idle");
    actor_ = request.actor;
    initial_ = ::placement(selectedMark(
        document, actor,
        mode_ == EditorCharacterPreviewMode::Clip ? request.mark : 0));
    placement_ = initial_;
    speed_ = actor.speed;
    walk_cycle_distance_ =
        findCharacterModel(actor.model)->walk_cycle_distance_m;
    if (mode_ == EditorCharacterPreviewMode::Route) {
      const auto route = std::get<CharacterRouteDefinition>(
          *document.object(document.selection()));
      for (const auto& id : route.marks)
        marks_.push_back(
            *findCharacterMark(document.document()->characters, id));
      final_clip_ = route.final_clip;
      stage_ = EditorRoutePreviewStage::Heading;
    }
    refresh();
    return true;
  } catch (const std::exception& error) {
    const std::string message = error.what();
    stop();
    error_ = message;
    return false;
  }
}

void EditorCharacterPreview::synchronize(const EditorDocument& document) {
  if ((active() || !error_.empty()) &&
      (document.generation() != generation_ ||
       document.revision() != revision_ || document.selection() != selection_ ||
       document.selectionRevision() != selection_revision_ ||
       document.pendingAction().kind != EditorPendingActionKind::None))
    stop();
}
void EditorCharacterPreview::stop() {
  error_.clear();
  playback_.reset();
  asset_.reset();
  palette_.clear();
  marks_.clear();
  final_clip_.reset();
  actor_ = editor_no_object;
  segment_ = 0;
  walked_distance_ = 0;
  stage_ = EditorRoutePreviewStage::Completed;
  placement_ = initial_ = {};
}
void EditorCharacterPreview::pause() {
  if (playback_) playback_->setPaused(!playback_->paused());
}
void EditorCharacterPreview::restart() {
  if (!playback_) return;
  if (mode_ == EditorCharacterPreviewMode::Route) {
    placement_ = initial_;
    segment_ = 0;
    walked_distance_ = 0;
    stage_ = EditorRoutePreviewStage::Heading;
    playback_->selectClip("idle");
  }
  playback_->restart();
  refresh();
}
void EditorCharacterPreview::seek(double seconds) {
  if (playback_ && mode_ == EditorCharacterPreviewMode::Clip) {
    playback_->seek(seconds);
    refresh();
  }
}
void EditorCharacterPreview::selectClip(std::string_view clip) {
  if (playback_ && mode_ == EditorCharacterPreviewMode::Clip) {
    playback_->selectClip(clip);
    refresh();
  }
}
double EditorCharacterPreview::duration() const {
  return playback_ ? characterClip(*asset_, playback_->clip()).duration : 0;
}
std::string_view EditorCharacterPreview::targetMark() const {
  return marks_.empty() ? std::string_view{}
                        : marks_[std::min(segment_, marks_.size() - 1)].id;
}
void EditorCharacterPreview::refresh() { palette_ = playback_->pose(); }
void EditorCharacterPreview::advance(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0)
    throw std::invalid_argument("Preview time must be finite and nonnegative.");
  if (!playback_ || playback_->paused() || seconds == 0) return;
  if (mode_ == EditorCharacterPreviewMode::Clip)
    (void)playback_->advance(seconds);
  else
    advanceRoute(seconds);
  refresh();
}

void EditorCharacterPreview::advanceRoute(double seconds) {
  // Consume exact stage durations so render batching cannot skip a mark or
  // facing. Elevation is interpolated between endpoints, without support tests.
  while (seconds > 0) {
    if (stage_ == EditorRoutePreviewStage::Completed) {
      (void)playback_->advance(seconds);
      break;
    }
    if (stage_ == EditorRoutePreviewStage::Interaction) {
      const double used = std::min(seconds, duration() - playback_->time());
      (void)playback_->advance(used);
      seconds -= used;
      if (playback_->time() >= duration()) {
        stage_ = EditorRoutePreviewStage::Completed;
        playback_->selectClip("idle");
      }
      continue;
    }
    const auto& target = marks_[segment_];
    const double dx =
        static_cast<double>(target.feet_position.x) - placement_.position[0];
    const double dy =
        static_cast<double>(target.feet_position.y) - placement_.position[1];
    const double dz =
        static_cast<double>(target.feet_position.z) - placement_.position[2];
    const double horizontal = std::hypot(dx, dz);
    const double length = std::hypot(horizontal, dy);
    if (stage_ == EditorRoutePreviewStage::Heading ||
        stage_ == EditorRoutePreviewStage::Facing) {
      const double target_yaw =
          stage_ == EditorRoutePreviewStage::Facing ? target.yaw_degrees
          : horizontal > 1e-6 ? std::atan2(dx, dz) * 180 / std::numbers::pi
                              : placement_.yaw_degrees;
      const double delta = yawDelta(placement_.yaw_degrees, target_yaw);
      const double used = std::min(seconds, std::abs(delta) / 120.);
      playback_->selectClip("idle");
      (void)playback_->advance(used);
      placement_.yaw_degrees = static_cast<float>(std::remainder(
          placement_.yaw_degrees + std::clamp(delta, -120 * used, 120 * used),
          360.));
      seconds -= used;
      if (used + 1e-9 < std::abs(delta) / 120.) break;
      placement_.yaw_degrees =
          static_cast<float>(std::remainder(target_yaw, 360.));
      if (stage_ == EditorRoutePreviewStage::Heading)
        stage_ = EditorRoutePreviewStage::Travel;
      else if (++segment_ < marks_.size())
        stage_ = EditorRoutePreviewStage::Heading;
      else {
        stage_ = final_clip_ ? EditorRoutePreviewStage::Interaction
                             : EditorRoutePreviewStage::Completed;
        playback_->selectClip(final_clip_ ? "interact" : "idle");
      }
      continue;
    }
    // Horizontal speed matches authored walk conventions; a purely vertical
    // diagram edge still consumes its geometric length and stays schematic.
    const double travel_length = horizontal > 1e-6 ? horizontal : length;
    const double used = std::min(seconds, travel_length / speed_);
    const double amount =
        travel_length > 0 ? std::min(1., speed_ * used / travel_length) : 1;
    placement_.position[0] += static_cast<float>(dx * amount);
    placement_.position[1] += static_cast<float>(dy * amount);
    placement_.position[2] += static_cast<float>(dz * amount);
    if (used > 0) {
      walked_distance_ += travel_length * amount;
      playback_->selectClip("walk");
      playback_->drive(walked_distance_ / walk_cycle_distance_ *
                           characterClip(*asset_, "walk").duration,
                       used);
    }
    seconds -= used;
    if (amount < 1) break;
    placement_.position = ::placement(target).position;
    stage_ = EditorRoutePreviewStage::Facing;
  }
}
