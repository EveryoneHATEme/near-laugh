#ifndef CORE_GAMEPLAY_NARRATIVE_PROGRESSION_HPP
#define CORE_GAMEPLAY_NARRATIVE_PROGRESSION_HPP

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/audio/cue_coordinator.hpp"
#include "core/gameplay/accepted_interaction.hpp"

struct NarrativeBooleanObservation {
  std::string id;
  bool value{};
};
struct NarrativeDoorObservation {
  std::string id;
  bool open{}, closed{}, locked{};
};
struct NarrativeCueObservation {
  std::string id;
  std::uint64_t instance{};
  CueStatus state{};
};
struct NarrativeActorObservation {
  std::string id;
  std::uint64_t instance{};
  NarrativeActorState state{};
};
struct NarrativeObservation {
  double active_time{};
  WorldPosition feet{};
  std::vector<NarrativeBooleanObservation> lights, radios, boxes, documents;
  std::vector<NarrativeDoorObservation> doors;
  std::vector<NarrativeActorObservation> actors;
  std::vector<NarrativeCueObservation> cues;
};
enum class NarrativeRunStatus { Dormant, Running, Completed, Canceled, Failed };
enum class NarrativeOwnedKind { Cue, Route };
struct NarrativeOwnedAction {
  NarrativeOwnedKind kind{};
  std::string target;
  std::uint64_t instance{};
};
struct NarrativeRunSnapshot {
  std::string event;
  std::uint64_t run{};
  std::size_t step{};
  NarrativeRunStatus status{};
  std::string reason, target;
  std::optional<NarrativeOwnedAction> owned;
};
struct NarrativeTransition {
  NarrativeRunSnapshot event;
  double active_time{};
};
enum class NarrativeCommandStatus { Accepted, Started, Busy, Refused };
struct NarrativeCommandResult {
  NarrativeCommandStatus status{NarrativeCommandStatus::Refused};
  std::uint64_t instance{};
  std::string reason;
};
struct NarrativeCommand {
  std::string event;
  std::uint64_t run{};
  std::size_t step{};
  NarrativeStep action;
};

// Device-free reducer. The caller applies all preflight cancellations before
// character audio handoff, then pairs each nextCommand with exactly one
// resolve.
class NarrativeProgression {
 public:
  NarrativeProgression(const LevelNarrative& definitions,
                       const NarrativeObservation& initial);
  void samplePosition(WorldPosition feet);
  [[nodiscard]] std::vector<NarrativeOwnedAction> beginBoundary(
      NarrativeObservation observation,
      std::span<const AcceptedInteraction> interactions = {});
  [[nodiscard]] std::optional<NarrativeCommand> nextCommand();
  void resolve(const NarrativeCommandResult& result);
  [[nodiscard]] std::vector<NarrativeOwnedAction> close();
  [[nodiscard]] const std::map<std::string, bool>& facts() const {
    return facts_;
  }
  [[nodiscard]] std::vector<NarrativeRunSnapshot> runs() const;
  [[nodiscard]] const std::deque<NarrativeTransition>& trace() const {
    return trace_;
  }
  [[nodiscard]] std::uint64_t droppedTraceRecords() const { return dropped_; }
  // Sticky diagnostic; the overflow itself affects only the next boundary.
  [[nodiscard]] bool observationOverflow() const { return overflow_; }

 private:
  struct Run {
    NarrativeRunSnapshot view;
    NarrativeRunStatus frozen_status{};
    bool previous_condition{}, condition_rearmed{true}, eligible{};
    bool owned_complete{};
    std::optional<double> deadline;
  };
  [[nodiscard]] bool holds(const NarrativePredicate& predicate) const;
  [[nodiscard]] bool condition(
      const std::vector<NarrativePredicate>& predicates) const;
  void transition(std::size_t event, std::string reason,
                  std::string target = {});
  void terminal(std::size_t event, NarrativeRunStatus status,
                std::string reason);
  void advanceStep(std::size_t event);
  const LevelNarrative& definitions_;
  NarrativeObservation snapshot_;
  std::map<std::string, bool> facts_, frozen_facts_, occupancy_;
  std::map<std::string, std::size_t, std::less<>> event_indices_;
  std::vector<Run> runs_;
  std::vector<std::size_t> order_;
  std::vector<std::string> entries_;
  std::deque<NarrativeTransition> trace_;
  std::uint64_t dropped_{}, consumed_interaction_{};
  double origin_{}, last_time_{};
  std::size_t cursor_{};
  std::optional<std::size_t> pending_;
  bool first_{true}, boundary_{}, closed_{}, overflow_{}, overflow_pending_{};
  bool facts_changed_{};
};

#endif
