#ifndef CORE_ENGINE_HPP
#define CORE_ENGINE_HPP

#include <utility>

#include "core/gameplay/prototype_rifle.hpp"
#include "core/gameplay/shooting_range.hpp"
#include "core/gameplay/shooting_targets.hpp"
#include "core/input/fps_input.hpp"
#include "core/physics/physics_world.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/player/player_controller.hpp"
#include "core/render/renderer.hpp"
#include "core/runtime_resources.hpp"
#include "core/simulation/fixed_step.hpp"
#include "core/world/prototype_level.hpp"
#include "near_laugh/runtime_config.hpp"

class ValidationDiagnostics;

class Engine {
 public:
  Engine(const near_laugh::RuntimeConfig& config, RuntimeResources resources,
         ValidationDiagnostics& diagnostics);
  ~Engine() = default;

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  Engine(Engine&&) = delete;
  Engine& operator=(Engine&&) = delete;

  void run();
  [[nodiscard]] bool tick();

 private:
  void samplePlayerInput(const FpsActionSnapshot& input);

  Platform platform_;
  Window window_;
  PrototypeLevel level_{};
  PhysicsWorld physics_;
  PlayerController player_;
  PrototypeRifle rifle_{};
  ShootingTargets targets_;
  Renderer renderer_;
  FpsInputMapper input_mapper_{};
  FpsActionSnapshot input_{};
  FixedStepAccumulator fixed_step_{};
};

#endif
