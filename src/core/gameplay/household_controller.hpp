#ifndef CORE_GAMEPLAY_HOUSEHOLD_CONTROLLER_HPP
#define CORE_GAMEPLAY_HOUSEHOLD_CONTROLLER_HPP

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/audio/cue_coordinator.hpp"
#include "core/player/player_controller.hpp"

enum class HouseholdCommandKind { Pickup, Drop, Throw };
enum class HouseholdResult {
  None,
  Queued,
  PickedUp,
  Dropped,
  Thrown,
  Refused,
  Busy,
  Released,
  Reading,
  Closed,
  RadioOn,
  RadioOff
};

class HouseholdController {
 public:
  HouseholdController(const PrototypeLevel& level, PhysicsWorld& physics,
                      CueCoordinator& audio);
  ~HouseholdController();
  HouseholdController(const HouseholdController&) = delete;
  HouseholdController& operator=(const HouseholdController&) = delete;

  // Every event batch, including inactive/minimized batches. The returned
  // controls have passed mode ownership and release gates; look is never
  // queued.
  [[nodiscard]] PlayerActionSnapshot sampleInput(
      const PlayerActionSnapshot& input, bool controls_active, bool suspended);
  [[nodiscard]] bool consumesEscape() const noexcept;
  [[nodiscard]] bool worldActionsAllowed() const noexcept;
  [[nodiscard]] bool preservesStance() const noexcept;
  void suspend();
  void stop();

  bool requestPickup(std::size_t box, const PlayerViewPose& selection);
  bool requestDrop();
  bool requestThrow(PhysicsVector direction);
  // Called exactly once before each shared world update, using the accepted
  // simulation eye/look. Ordinary commands are consumed here at most once.
  void beforeFixedStep(const PlayerViewPose& simulation_view);
  [[nodiscard]] bool commandPending() const noexcept {
    return pending_.has_value();
  }
  [[nodiscard]] bool safetyReleaseOwed() const noexcept {
    return safety_release_;
  }
  [[nodiscard]] std::optional<std::size_t> heldBox() const noexcept;

  void openDocument(std::size_t document);
  [[nodiscard]] std::optional<std::size_t> readingDocument() const noexcept;
  [[nodiscard]] std::size_t page() const noexcept { return page_; }
  void toggleRadio(std::size_t radio);
  [[nodiscard]] bool radioOn(std::size_t radio) const;
  [[nodiscard]] bool ownsSource(std::string_view source) const noexcept;

  void hint(std::string_view text);
  void feedback(HouseholdResult result, std::string_view text);
  [[nodiscard]] HouseholdResult result() const noexcept { return result_; }
  [[nodiscard]] HouseholdTextPresentation text();
  [[nodiscard]] std::span<const OpaqueBoxFrame> presentation(
      std::span<const OpaqueBoxFrame> doors);

 private:
  struct PendingCommand {
    HouseholdCommandKind kind{};
    std::size_t box{};
    PlayerViewPose selection{};
    PhysicsVector throw_direction{};
  };
  bool queue(PendingCommand command);
  void blockHeldControls();
  void closeDocument();

  const PrototypeLevel& level_;
  PhysicsWorld& physics_;
  CueCoordinator& audio_;
  std::vector<const PrototypeStaticProp*> radio_props_;
  std::vector<bool> radio_enabled_;
  std::optional<PendingCommand> pending_;
  std::optional<std::size_t> document_;
  std::size_t page_{};
  std::array<float, 4> hold_relative_orientation_{0, 0, 0, 1};
  PlayerActionSnapshot last_input_{};
  std::array<bool, 12> blocked_{};
  std::array<bool, 4> reader_armed_{};
  bool active_{}, suspended_{}, mode_changed_{}, safety_release_{};
  bool reader_escape_held_{};
  HouseholdResult result_{};
  std::string hint_, feedback_, page_controls_;
  double feedback_until_{};
  std::vector<OpaqueBoxFrame> boxes_;
};

#endif
