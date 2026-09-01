#ifndef CORE_INPUT_FPS_INPUT_HPP
#define CORE_INPUT_FPS_INPUT_HPP

#include "core/platform/input.hpp"

struct FpsActionSnapshot {
  bool move_forward{};
  bool move_backward{};
  bool move_left{};
  bool move_right{};
  bool jump{};
  bool sprint{};
  bool crouch{};
  bool menu{};
  bool primary_action{};
  bool secondary_action{};
  double look_delta_x{};
  double look_delta_y{};
};

class FpsInputMapper {
 public:
  [[nodiscard]] FpsActionSnapshot map(
      const PhysicalInputSnapshot& physical) const noexcept;
};

#endif
