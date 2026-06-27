#ifndef CORE_GAME_GAME_WORLD_HPP
#define CORE_GAME_GAME_WORLD_HPP

#include <cstddef>
#include <vector>

#include "../input/input_state.hpp"
#include "../math/geometry.hpp"
#include "../render/render_types.hpp"
#include "level.hpp"
#include "player_controller.hpp"

struct Target {
  math::AABB bounds{};
  int health{1};
  float hit_flash{0.0f};

  bool alive() const { return health > 0; }
};

class GameWorld {
 private:
  Level level;
  PlayerController player;
  std::vector<Target> targets;
  float crosshair_flash{0.0f};

  void fireHitscan();

 public:
  GameWorld();

  void update(float dt, const InputState& input);
  RenderPacket render(float aspect_ratio) const;

  const PlayerController& getPlayer() const { return player; }
  PlayerController& getPlayer() { return player; }
  const std::vector<Target>& getTargets() const { return targets; }
  std::size_t aliveTargetCount() const;
};

#endif
