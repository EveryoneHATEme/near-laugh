#include "level.hpp"

void Level::addBox(const StaticBox& box) {
  boxes.push_back(box);
  if (box.blocks_player) {
    player_colliders.push_back(box.bounds);
  }
  if (box.blocks_shots) {
    shot_blockers.push_back(box.bounds);
  }
}

Level Level::createPrototypeArena() {
  Level level;

  level.addBox({{{-8.0f, -0.1f, -8.0f}, {8.0f, 0.0f, 8.0f}},
                {92, 96, 105, 255},
                false,
                false});

  level.addBox({{{-8.25f, 0.0f, -8.25f}, {8.25f, 2.8f, -8.0f}},
                {72, 91, 122, 255}});
  level.addBox({{{-8.25f, 0.0f, 8.0f}, {8.25f, 2.8f, 8.25f}},
                {72, 91, 122, 255}});
  level.addBox({{{-8.25f, 0.0f, -8.25f}, {-8.0f, 2.8f, 8.25f}},
                {82, 111, 112, 255}});
  level.addBox({{{8.0f, 0.0f, -8.25f}, {8.25f, 2.8f, 8.25f}},
                {82, 111, 112, 255}});

  level.addBox({{{-3.0f, 0.0f, -2.5f}, {-1.4f, 1.25f, 1.3f}},
                {128, 109, 84, 255}});
  level.addBox({{{2.0f, 0.0f, 0.8f}, {4.4f, 1.5f, 2.1f}},
                {112, 84, 122, 255}});
  level.addBox({{{-0.6f, 0.0f, -5.7f}, {0.6f, 0.9f, -4.9f}},
                {94, 116, 78, 255}});

  return level;
}
