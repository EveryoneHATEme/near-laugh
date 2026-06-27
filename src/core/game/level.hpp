#ifndef CORE_GAME_LEVEL_HPP
#define CORE_GAME_LEVEL_HPP

#include <vector>

#include "../math/geometry.hpp"
#include "../render/render_types.hpp"

struct StaticBox {
  math::AABB bounds{};
  Color color{};
  bool blocks_player{true};
  bool blocks_shots{true};
};

class Level {
 private:
  std::vector<StaticBox> boxes;
  std::vector<math::AABB> player_colliders;
  std::vector<math::AABB> shot_blockers;

 public:
  static Level createPrototypeArena();

  const std::vector<StaticBox>& getBoxes() const { return boxes; }
  const std::vector<math::AABB>& getPlayerColliders() const {
    return player_colliders;
  }
  const std::vector<math::AABB>& getShotBlockers() const {
    return shot_blockers;
  }

  void addBox(const StaticBox& box);
};

#endif
