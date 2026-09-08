#ifndef CORE_GAMEPLAY_CHARACTER_CONTROLLER_HPP
#define CORE_GAMEPLAY_CHARACTER_CONTROLLER_HPP

#include "core/animation/character_scene.hpp"
#include "core/audio/cue_coordinator.hpp"
#include "core/frame.hpp"
#include "core/physics/physics_world.hpp"
#include "core/render/character_presentation.hpp"
#include "core/world/characters.hpp"

struct FrameTimingSample;

enum class CharacterStart { Started, AlreadyActive, Busy };
enum class CharacterAction {
  Idle,
  Turning,
  Walking,
  Blocked,
  Interacting,
  Completed,
  Canceled
};
struct CharacterActionResult {
  std::string_view actor{};
  std::string_view route{};
  std::string_view mark{};
  std::uint64_t instance{};
  CharacterAction action{CharacterAction::Idle};
  PhysicsActorObstruction obstruction{PhysicsActorObstruction::None};
  bool audio_contention{};
  double walked_distance{};
  std::uint64_t contacts{};
};

// Concrete authored routes. Definitions, physics and audio outlive this owner.
// Engine advances the world/player, calls fixedStep, then advances doors and
// calls handoffAudio once for the accepted batch. No wall clock moves an actor.
class CharacterController {
 public:
  CharacterController(
      const LevelCharacters& definitions,
      std::span<const std::shared_ptr<const CharacterAsset>> assets,
      PhysicsWorld& physics, CueCoordinator& audio);
  [[nodiscard]] CharacterStart start(std::string_view actor,
                                     std::string_view route);
  bool cancel(std::string_view actor);
  void fixedStep(float seconds, FrameTimingSample* timings = nullptr);
  void handoffAudio();
  [[nodiscard]] const CharacterActionResult& result(std::size_t actor) const;
  [[nodiscard]] const CharacterPlayback& playback(std::size_t actor) const;
  [[nodiscard]] std::span<const CharacterPoseFrame> presentation()
      const noexcept {
    return frames_;
  }
  [[nodiscard]] std::vector<CharacterRenderInstance> renderInstances() const;
  [[nodiscard]] bool ownsSource(std::string_view source) const noexcept;

 private:
  enum class Stage { Inactive, Heading, Travel, Facing, Interaction };
  struct Actor {
    const CharacterCatalogEntry* profile{};
    std::shared_ptr<const CharacterAsset> asset;
    CharacterPlayback playback;
    CharacterPose palette{};
    CharacterActionResult result{};
    const CharacterRouteDefinition* route{};
    std::size_t mark{};
    Stage stage{Stage::Inactive};
    bool interaction_fired{}, pending_interaction{};
    std::uint64_t footstep_instance{}, interaction_instance{};
    Actor(const CharacterCatalogEntry* profile,
          std::shared_ptr<const CharacterAsset> asset);
  };
  struct Contact {
    std::size_t actor;
    std::uint64_t action;
  };
  [[nodiscard]] std::size_t actorIndex(std::string_view id) const;
  void cancelSounds(std::size_t actor);
  void publish(std::size_t actor);
  void nextMark(Actor& actor);
  const LevelCharacters& definitions_;
  PhysicsWorld& physics_;
  CueCoordinator& audio_;
  std::vector<Actor> actors_;
  std::vector<std::size_t> order_;
  std::vector<CharacterPoseFrame> frames_;
  std::vector<Contact> contacts_;
};

#endif
