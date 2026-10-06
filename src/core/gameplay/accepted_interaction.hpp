#ifndef CORE_GAMEPLAY_ACCEPTED_INTERACTION_HPP
#define CORE_GAMEPLAY_ACCEPTED_INTERACTION_HPP

#include <array>
#include <span>
#include <stdexcept>
#include <string_view>

#include "core/simulation/fixed_step.hpp"
#include "core/world/level_document.hpp"

enum class AcceptedInteractionResult {
  Opening,
  Closing,
  Locked,
  Unlocked,
  Knocked,
  Enabled,
  Disabled,
  Opened,
  PickedUp,
  Dropped,
  Thrown
};
struct AcceptedInteraction {
  std::uint64_t occurrence{};
  NarrativeInteractionTarget target_kind{};
  std::string_view target;
  NarrativeInteractionAction action{};
  AcceptedInteractionResult result{};
};

// At most one physical command per accepted step and one ordinary interaction
// per batch. Retain accepted outcomes across suspension; only consumption
// clears.
class AcceptedInteractions {
 public:
  static constexpr std::size_t capacity =
      FixedStepAccumulator::maximum_steps_per_sample + 1;
  void publish(NarrativeInteractionTarget kind, std::string_view target,
               NarrativeInteractionAction action,
               AcceptedInteractionResult result) {
    if (size_ == capacity)
      throw std::overflow_error(
          "Accepted interaction observation capacity exceeded");
    values_[size_++] = {++serial_, kind, target, action, result};
  }
  [[nodiscard]] std::span<const AcceptedInteraction> pending() const {
    return {values_.data(), size_};
  }
  void consume() noexcept { size_ = 0; }

 private:
  std::array<AcceptedInteraction, capacity> values_{};
  std::size_t size_{};
  std::uint64_t serial_{};
};

#endif
