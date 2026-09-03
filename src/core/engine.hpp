#ifndef CORE_ENGINE_HPP
#define CORE_ENGINE_HPP

#include "core/gameplay/player_flashlight.hpp"
#include "core/input/player_input.hpp"
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
  Engine(const near_laugh::RuntimeConfig& config,
         ValidationDiagnostics& diagnostics);
  ~Engine() = default;

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  Engine(Engine&&) = delete;
  Engine& operator=(Engine&&) = delete;

  void run();
  [[nodiscard]] bool tick();

 private:
  void samplePlayerInput(const PlayerActionSnapshot& input);

  Platform platform_;
  Window window_;
  RuntimeResources resources_;
  PrototypeLevel level_;
  PhysicsWorld physics_;
  PlayerController player_;
  PlayerFlashlight flashlight_{};
  Renderer renderer_;
  PlayerInputMapper input_mapper_{};
  PlayerActionSnapshot input_{};
  FixedStepAccumulator fixed_step_{};
};

#endif
