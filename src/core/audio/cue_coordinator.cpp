#include "core/audio/cue_coordinator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "core/audio/transmission.hpp"

namespace {
LevelAudio checkedDefinitions(LevelAudio definitions,
                              const std::vector<DoorDefinition>& doors,
                              double now) {
  const auto errors = validateLevelAudio(definitions, doors);
  if (!errors.empty())
    throw std::invalid_argument(formatLevelDiagnostics(errors));
  if (!std::isfinite(now))
    throw std::invalid_argument("cue clock must be finite");
  return definitions;
}
}  // namespace

CueCoordinator::CueCoordinator(LevelAudio definitions,
                               std::vector<DoorDefinition> doors,
                               AudioContent content, AudioOutput output,
                               double now)
    : definitions_(checkedDefinitions(std::move(definitions), doors, now)),
      doors_(std::move(doors)),
      playback_(std::move(content), output),
      instances_(definitions_.sources.size()),
      last_time_(now) {
  for (std::size_t i = 0; i < instances_.size(); ++i) {
    instances_[i].position = definitions_.sources[i].position;
    static_cast<void>(playback_.content().clip(cue(i).clip));
  }
}
std::size_t CueCoordinator::index(std::string_view source) const {
  for (std::size_t i = 0; i < definitions_.sources.size(); ++i)
    if (definitions_.sources[i].id == source) return i;
  throw std::invalid_argument("unknown audio source '" + std::string(source) +
                              "'");
}
const AudioCueDefinition& CueCoordinator::cue(std::size_t source) const {
  return *findAudioCue(definitions_, definitions_.sources[source].cue);
}
CueStart CueCoordinator::start(std::string_view source) {
  const auto i = index(source);
  auto& current = instances_[i];
  if (current.status == CueStatus::Playing) return CueStart::AlreadyActive;
  const auto& definition = cue(i);
  const bool foreground = definition.kind != AudioCueKind::Ambience;
  if (foreground && foreground_) return CueStart::Busy;
  auto placement = definitions_.sources[i];
  placement.position = current.position;
  try {
    playback_.start(i, definition, placement);
  } catch (const std::runtime_error& e) {
    playback_.silentFallback("Audio voice failed; captions continue: " +
                             std::string(e.what()));
  }
  current.status = CueStatus::Playing;
  current.start = active_time_;
  current.serial = ++next_serial_;
  current.synchronization_failures = 0;
  current.correction_time.reset();
  if (foreground) foreground_ = i;
  updateGains();
  return CueStart::Started;
}
bool CueCoordinator::cancel(std::string_view source) {
  const auto i = index(source);
  if (instances_[i].status != CueStatus::Playing) return false;
  playback_.stop(i);
  instances_[i].status = CueStatus::Cancelled;
  if (foreground_ == i) foreground_.reset();
  updateGains();
  return true;
}
void CueCoordinator::cancelAll() {
  for (const auto& source : definitions_.sources) cancel(source.id);
}
void CueCoordinator::autoplay() {
  for (const auto& source : definitions_.sources)
    if (source.autoplay) static_cast<void>(start(source.id));
}
void CueCoordinator::update(double now) {
  if (!std::isfinite(now) || now < last_time_)
    throw std::invalid_argument("cue clock must be finite and monotonic");
  if (!suspended_) active_time_ += now - last_time_;
  last_time_ = now;
  playback_.pollDevice();
  for (std::size_t i = 0; i < instances_.size(); ++i) {
    auto& current = instances_[i];
    if (current.status != CueStatus::Playing) continue;
    if (!cue(i).loop && active_time_ - current.start >=
                            playback_.content().clip(cue(i).clip).duration()) {
      playback_.stop(i);
      current.status = CueStatus::Completed;
      if (foreground_ == i) foreground_.reset();
    } else if (!suspended_)
      synchronize(i, false);
  }
  updateGains();
}
void CueCoordinator::synchronize(std::size_t i, bool force) {
  if (playback_.silent()) return;
  const auto duration = playback_.content().clip(cue(i).clip).duration();
  const double elapsed = active_time_ - instances_[i].start;
  const double expected = cue(i).loop ? std::fmod(elapsed, duration) : elapsed;
  const double cursor = playback_.cursor(i);
  double drift = std::abs(cursor - expected);
  if (cue(i).loop && cursor >= 0)
    drift = std::min(drift, std::abs(duration - drift));
  if (force || cursor < 0 || drift > .1) {
    auto& current = instances_[i];
    // Seeking is consumed by a later mixer callback. Several fast presentation
    // frames before that callback are one attempt, not repeated failures.
    if (!force && current.correction_time &&
        active_time_ - *current.correction_time < .1)
      return;
    current.correction_time = active_time_;
    if (!playback_.seek(i, expected)) {
      if (++instances_[i].synchronization_failures >= 3)
        playback_.silentFallback(
            "Audio cursor cannot synchronize; continuing captions silently. "
            "Restart to retry.");
    } else {
      // Seek is processed on the next audio read. Consecutive corrections with
      // no convergence also indicate a stalled device, even if seek is
      // accepted.
      if (!force && ++instances_[i].synchronization_failures >= 3)
        playback_.silentFallback(
            "Audio cursor repeatedly drifted by over 100 ms; continuing "
            "captions silently. Restart to retry.");
    }
  } else {
    instances_[i].synchronization_failures = 0;
    instances_[i].correction_time.reset();
  }
}
void CueCoordinator::suspend(bool suspended, double now) {
  update(now);
  if (suspended_ == suspended) return;
  suspended_ = suspended;
  if (!suspended)
    for (std::size_t i = 0; i < instances_.size(); ++i)
      if (instances_[i].status == CueStatus::Playing) synchronize(i, true);
  playback_.suspend(suspended);
}
void CueCoordinator::mute(bool muted) {
  muted_ = muted;
  playback_.mute(muted);
  updateGains();
}
void CueCoordinator::listener(WorldPosition position, WorldPosition direction) {
  playback_.listener(position, direction);
  listener_ = position;
  updateGains();
}
void CueCoordinator::moveSource(std::string_view source,
                                WorldPosition position) {
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z))
    throw std::invalid_argument("audio source position must be finite");
  instances_[index(source)].position = position;
  updateGains();
}
void CueCoordinator::acceptedDoors(std::span<const float> angles) {
  if (!angles.empty() && angles.size() != doors_.size())
    throw std::invalid_argument("missing accepted door angles");
  for (const float angle : angles)
    if (!std::isfinite(angle))
      throw std::invalid_argument("nonfinite accepted door angle");
  accepted_angles_.assign(angles.begin(), angles.end());
  updateGains();
}
void CueCoordinator::updateGains() {
  for (std::size_t i = 0; i < instances_.size(); ++i) {
    auto& current = instances_[i];
    const auto& source = definitions_.sources[i];
    const auto& c = cue(i);
    float gain = source.gain;
    if (c.spatial)
      gain *= audioTransmission(definitions_, doors_, accepted_angles_,
                                current.position, listener_);
    if (foreground_ && c.kind == AudioCueKind::Ambience) gain *= .25F;
    playback_.source(i, current.position, gain);
    current.effective_gain =
        muted_
            ? 0
            : gain * (c.spatial ? audioDistanceGain(current.position, listener_,
                                                    source.near_distance,
                                                    source.far_distance)
                                : 1.0F);
  }
}
CueStatus CueCoordinator::status(std::string_view source) const {
  return instances_[index(source)].status;
}
double CueCoordinator::offset(std::string_view source) const {
  const auto& current = instances_[index(source)];
  return active_time_ - current.start;
}
std::uint64_t CueCoordinator::instance(std::string_view source) const {
  return instances_[index(source)].serial;
}
float CueCoordinator::effectiveGain(std::string_view source) const {
  return instances_[index(source)].effective_gain;
}
CaptionPresentation CueCoordinator::captions() const {
  CaptionPresentation result;
  std::optional<std::size_t> ambience;
  for (std::size_t i = 0; i < instances_.size(); ++i) {
    const auto& current = instances_[i];
    const auto& c = cue(i);
    if (current.status != CueStatus::Playing || !c.caption) continue;
    const auto* track = playback_.content().caption(*c.caption);
    if (!track) continue;
    const double elapsed = active_time_ - current.start;
    for (const auto& segment : track->segments) {
      if (elapsed < segment.start || elapsed >= segment.end) continue;
      if (c.kind != AudioCueKind::Ambience)
        result.foreground = {segment.label, segment.text};
      else if (!ambience ||
               current.effective_gain > instances_[*ambience].effective_gain ||
               (current.effective_gain ==
                    instances_[*ambience].effective_gain &&
                definitions_.sources[i].id <
                    definitions_.sources[*ambience].id)) {
        ambience = i;
        result.ambience = {segment.label, segment.text};
      }
    }
  }
  return result;
}
