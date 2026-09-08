#ifndef CORE_ENGINE_HPP
#define CORE_ENGINE_HPP

#include "core/audio/apartment_audio_fixture.hpp"
#include "core/gameplay/authored_interaction.hpp"
#include "core/gameplay/character_controller.hpp"
#include "core/gameplay/light_switch_controller.hpp"
#include "core/gameplay/player_flashlight.hpp"
#include "core/input/player_input.hpp"
#include "core/physics/physics_world.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/player/player_controller.hpp"
#include "core/render/renderer.hpp"
#include "core/runtime_resources.hpp"
#include "core/simulation/fixed_step.hpp"
#include "core/text/caption_font.hpp"
#include "core/world/prototype_level.hpp"
#include "near_laugh/runtime_config.hpp"

class ValidationDiagnostics;

struct CharacterDevelopmentInput {
  bool restart{}, cancel{}, pause{}, mute{};
};

class Engine {
 public:
  Engine(const near_laugh::RuntimeConfig& config,
         ValidationDiagnostics& diagnostics, bool audio_fixture = false,
         AudioOutput output = AudioOutput::Device,
         FrameTimings* timings = nullptr, bool character_fixture = false,
         FrameCapture* capture = nullptr);
  ~Engine() = default;

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  Engine(Engine&&) = delete;
  Engine& operator=(Engine&&) = delete;

  void run();
  [[nodiscard]] bool tick(
      const PlayerActionSnapshot* development_input = nullptr,
      const CharacterDevelopmentInput* character_input = nullptr);

 private:
  friend struct EngineAudioSmoke;
  friend struct InteriorLightingMeasurement;
  friend struct EngineCharacterSmoke;
  bool samplePlayerInput(const PlayerActionSnapshot& input);
  void sampleFixtureControls(bool active, double now,
                             const CharacterDevelopmentInput* input = nullptr);
  void suspendWorld(bool suspended, double now);

  Platform platform_;
  Window window_;
  RuntimeResources resources_;
  PrototypeLevel level_;
  const LevelEntry& entry_;
  std::shared_ptr<const CaptionFont> caption_font_;
  std::vector<std::shared_ptr<const CharacterAsset>> character_assets_;
  CueCoordinator audio_;
  std::optional<ApartmentAudioFixture> audio_fixture_;
  std::string audio_warning_;
  PhysicsWorld physics_;
  PlayerController player_;
  PlayerFlashlight flashlight_{};
  LightSwitchController light_switch_;
  DoorController doors_;
  CharacterController characters_;
  AuthoredInteraction interaction_{};
  Renderer renderer_;
  PlayerInputMapper input_mapper_{};
  PlayerActionSnapshot input_{};
  FixedStepAccumulator fixed_step_{};
  bool character_fixture_{}, character_paused_{}, suspended_{};
  CharacterDevelopmentInput previous_character_input_{};
};

#endif
