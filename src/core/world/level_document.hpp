#ifndef CORE_WORLD_LEVEL_DOCUMENT_HPP
#define CORE_WORLD_LEVEL_DOCUMENT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

inline constexpr std::uint32_t level_format_version = 11;
inline constexpr std::size_t level_maximum_door_count = 32;
inline constexpr std::size_t prototype_surface_count = 3;
inline constexpr std::size_t level_maximum_point_light_count = 8;
inline constexpr std::size_t level_maximum_light_switch_count = 16;
inline constexpr std::size_t level_maximum_shadow_light_count = 4;
inline constexpr std::size_t prototype_terrain_sample_count = 97;
inline constexpr std::size_t prototype_terrain_cell_count =
    prototype_terrain_sample_count - 1;
inline constexpr std::size_t level_maximum_solid_count = 240;
inline constexpr std::size_t level_maximum_entry_count = 16;
inline constexpr std::size_t level_maximum_entry_id_length = 64;
inline constexpr float prototype_terrain_sample_spacing = 0.5F;
inline constexpr float prototype_terrain_maximum_slope_degrees = 50.0F;
inline constexpr float prototype_maximum_ambient_intensity = 0.20F;
inline constexpr float prototype_spawn_validation_radius = 0.35F;
inline constexpr float prototype_spawn_validation_height = 1.80F;
// Character collision skin, shared by authored door clearance and physics.
inline constexpr float prototype_player_contact_padding = 0.02F;

struct WorldPosition {
  bool operator==(const WorldPosition&) const = default;
  float x{};
  float y{};
  float z{};
};

struct WorldExtent {
  bool operator==(const WorldExtent&) const = default;
  float x{};
  float y{};
  float z{};
};

using WorldColor = std::array<std::uint8_t, 4>;

enum class PrototypeSolidKind {
  Floor,
  Boundary,
  Obstacle,
  WalkableStep,
  LowClearance,
};

enum class PrototypeSurface : std::uint32_t {
  Floor = 0,
  Boundary = 1,
  Obstacle = 2,
};

struct PrototypeSolid {
  bool operator==(const PrototypeSolid&) const = default;
  WorldPosition center{};
  WorldExtent half_extent{};
  WorldColor color{};
  PrototypeSolidKind kind{PrototypeSolidKind::Obstacle};
  std::string material{"prototype-obstacle"};
};

struct PrototypeTerrain {
  bool operator==(const PrototypeTerrain&) const = default;
  WorldPosition origin{};
  float sample_spacing{};
  std::array<float,
             prototype_terrain_sample_count * prototype_terrain_sample_count>
      heights{};
  std::string material{"prototype-floor"};
};

struct PrototypePlayerSpawn {
  bool operator==(const PrototypePlayerSpawn&) const = default;
  WorldPosition foot_position{};
  float yaw_degrees{};
};

struct LevelEntry {
  bool operator==(const LevelEntry&) const = default;
  std::string id{};
  PrototypePlayerSpawn pose{};
};

struct PrototypePointLight {
  bool operator==(const PrototypePointLight&) const = default;
  WorldPosition position{};
  std::array<float, 3> color{};
  float intensity{};
  float radius{};
  std::string id{};
  bool initially_on{true};
  bool casts_shadows{};
};

struct PrototypeEnvironmentLight {
  bool operator==(const PrototypeEnvironmentLight&) const = default;
  std::vector<PrototypePointLight> point_lights{};
  float ambient_intensity{};
};

inline constexpr std::size_t level_maximum_prop_count = 128;
inline constexpr std::size_t level_maximum_prop_box_count = 8;

struct PropCollisionBox {
  bool operator==(const PropCollisionBox&) const = default;
  WorldPosition center{};
  WorldExtent half_extent{};
};

struct PrototypeStaticProp {
  bool operator==(const PrototypeStaticProp&) const = default;
  std::string id{};
  std::string model{};
  WorldPosition translation{};
  float yaw_degrees{};
  float uniform_scale{1.0F};
  std::vector<PropCollisionBox> collision_boxes{};
};

struct PrototypeLightSwitch {
  bool operator==(const PrototypeLightSwitch&) const = default;
  WorldPosition position{};
  float yaw_degrees{};
  std::string light_id{};
  std::string id{};
};

enum class DoorLockSide { None, PositiveZ, NegativeZ };

struct DoorDefinition {
  bool operator==(const DoorDefinition&) const = default;
  std::string id{};
  WorldPosition hinge_position{};
  float closed_yaw_degrees{};
  float width{0.9F};
  float height{2.0F};
  float thickness{0.06F};
  float open_angle_degrees{90.0F};
  float speed_degrees_per_second{90.0F};
  DoorLockSide lock_side{DoorLockSide::PositiveZ};
  bool initially_open{};
  bool initially_locked{};
};

inline constexpr std::size_t level_maximum_audio_cue_count = 128;
inline constexpr std::size_t level_maximum_audio_source_count = 64;
inline constexpr std::size_t level_maximum_audio_room_count = 32;
inline constexpr std::size_t level_maximum_audio_connection_count = 64;

enum class AudioCueKind { Dialogue, Essential, Ambience };

struct AudioCueDefinition {
  bool operator==(const AudioCueDefinition&) const = default;
  std::string id{};
  std::string clip{};
  std::optional<std::string> caption{};
  AudioCueKind kind{AudioCueKind::Ambience};
  bool loop{};
  bool spatial{true};
};

struct AudioSourceDefinition {
  bool operator==(const AudioSourceDefinition&) const = default;
  std::string id{};
  std::string cue{};
  WorldPosition position{};
  float gain{1.0F};
  float near_distance{1.0F};
  float far_distance{20.0F};
  bool autoplay{};
};

struct AudioRoomDefinition {
  bool operator==(const AudioRoomDefinition&) const = default;
  std::string id{};
  WorldPosition center{};
  WorldExtent half_extent{1.0F, 1.0F, 1.0F};
};

struct AudioConnectionDefinition {
  bool operator==(const AudioConnectionDefinition&) const = default;
  std::string id{};
  std::optional<std::string> room_a{};
  std::optional<std::string> room_b{};
  std::optional<std::string> door{};
  float closed_gain{0.15F};
  float open_gain{1.0F};
};

struct LevelAudio {
  bool operator==(const LevelAudio&) const = default;
  std::vector<AudioCueDefinition> cues{};
  std::vector<AudioSourceDefinition> sources{};
  std::vector<AudioRoomDefinition> rooms{};
  std::vector<AudioConnectionDefinition> connections{};
};

inline constexpr std::size_t level_maximum_actor_count = 4;
inline constexpr std::size_t level_maximum_character_mark_count = 32;
inline constexpr std::size_t level_maximum_character_route_count = 16;
inline constexpr std::size_t level_maximum_route_mark_count = 32;

struct CharacterActorDefinition {
  bool operator==(const CharacterActorDefinition&) const = default;
  std::string id{};
  std::string model{};
  std::string initial_mark{};
  std::optional<std::string> initial_route{};
  float speed{1.0F};
  std::optional<std::string> footstep_source{};
  std::optional<std::string> interaction_source{};
};

struct CharacterMarkDefinition {
  bool operator==(const CharacterMarkDefinition&) const = default;
  std::string id{};
  WorldPosition feet_position{};
  float yaw_degrees{};
};

struct CharacterRouteDefinition {
  bool operator==(const CharacterRouteDefinition&) const = default;
  std::string id{};
  std::string actor{};
  std::vector<std::string> marks{};
  std::optional<std::string> final_clip{};
};

struct LevelCharacters {
  bool operator==(const LevelCharacters&) const = default;
  std::vector<CharacterActorDefinition> actors{};
  std::vector<CharacterMarkDefinition> marks{};
  std::vector<CharacterRouteDefinition> routes{};
};

inline constexpr std::size_t level_maximum_household_box_count = 16;
inline constexpr std::size_t level_maximum_household_document_count = 32;
inline constexpr std::size_t level_maximum_household_radio_count = 8;
inline constexpr float household_box_half_extent = 0.15F;
inline constexpr float household_box_mass = 1.0F;
inline constexpr std::size_t level_maximum_document_page_count = 16;
inline constexpr std::size_t level_maximum_document_title_scalars = 80;
inline constexpr std::size_t level_maximum_document_page_scalars = 480;

struct HouseholdBoxDefinition {
  bool operator==(const HouseholdBoxDefinition&) const = default;
  std::string id{};
  WorldPosition center{};
  float yaw_degrees{};
};

struct HouseholdDocumentDefinition {
  bool operator==(const HouseholdDocumentDefinition&) const = default;
  std::string id{};
  WorldPosition position{};
  float yaw_degrees{};
  std::string title{};
  std::vector<std::string> pages{};
};

struct HouseholdRadioDefinition {
  bool operator==(const HouseholdRadioDefinition&) const = default;
  std::string id{};
  std::string prop{};
  std::string source{};
  bool initially_on{};
};

struct LevelHousehold {
  bool operator==(const LevelHousehold&) const = default;
  std::vector<HouseholdBoxDefinition> boxes{};
  std::vector<HouseholdDocumentDefinition> documents{};
  std::vector<HouseholdRadioDefinition> radios{};
};

inline constexpr std::size_t level_maximum_narrative_fact_count = 32;
inline constexpr std::size_t level_maximum_narrative_region_count = 32;
inline constexpr std::size_t level_maximum_narrative_event_count = 64;
inline constexpr std::size_t level_maximum_narrative_step_count = 32;
inline constexpr std::size_t level_maximum_narrative_predicate_count = 8;
inline constexpr float level_maximum_narrative_seconds = 3600.F;

enum class NarrativeActorState {
  Idle,
  Turning,
  Walking,
  Blocked,
  Interacting,
  Completed,
  Canceled
};
enum class NarrativeEventTerminalState { Completed, Canceled, Failed };
enum class NarrativeRepeat { Once, Rearm };
enum class NarrativeInteractionTarget { Door, Switch, Radio, Document, Box };
enum class NarrativeInteractionAction {
  DoorInteract,
  DoorLock,
  DoorKnock,
  SwitchActivate,
  RadioOn,
  RadioOff,
  DocumentOpen,
  BoxPickup,
  BoxDrop,
  BoxThrow
};

// Per-kind records prevent authored data from carrying irrelevant parameters.
struct NarrativeFactPredicate {
  bool operator==(const NarrativeFactPredicate&) const = default;
  std::string fact{};
  bool value{};
};
struct NarrativeRegionPredicate {
  bool operator==(const NarrativeRegionPredicate&) const = default;
  std::string region{};
  bool inside{};
};
struct NarrativeLightPredicate {
  bool operator==(const NarrativeLightPredicate&) const = default;
  std::string light{};
  bool enabled{};
};
struct NarrativeDoorEndpointPredicate {
  bool operator==(const NarrativeDoorEndpointPredicate&) const = default;
  std::string door{};
  bool open{};
};
struct NarrativeDoorLockedPredicate {
  bool operator==(const NarrativeDoorLockedPredicate&) const = default;
  std::string door{};
  bool locked{};
};
struct NarrativeRadioPredicate {
  bool operator==(const NarrativeRadioPredicate&) const = default;
  std::string radio{};
  bool enabled{};
};
struct NarrativeBoxPredicate {
  bool operator==(const NarrativeBoxPredicate&) const = default;
  std::string box{};
  bool held{};
};
struct NarrativeDocumentPredicate {
  bool operator==(const NarrativeDocumentPredicate&) const = default;
  std::string document{};
  bool open{};
};
struct NarrativeActorPredicate {
  bool operator==(const NarrativeActorPredicate&) const = default;
  std::string actor{};
  NarrativeActorState state{NarrativeActorState::Idle};
};
struct NarrativeEventPredicate {
  bool operator==(const NarrativeEventPredicate&) const = default;
  std::string event{};
  NarrativeEventTerminalState state{NarrativeEventTerminalState::Completed};
};
struct NarrativeElapsedPredicate {
  bool operator==(const NarrativeElapsedPredicate&) const = default;
  float seconds{};
};
using NarrativePredicate =
    std::variant<NarrativeFactPredicate, NarrativeRegionPredicate,
                 NarrativeLightPredicate, NarrativeDoorEndpointPredicate,
                 NarrativeDoorLockedPredicate, NarrativeRadioPredicate,
                 NarrativeBoxPredicate, NarrativeDocumentPredicate,
                 NarrativeActorPredicate, NarrativeEventPredicate,
                 NarrativeElapsedPredicate>;

struct NarrativeSceneEntryTrigger {
  bool operator==(const NarrativeSceneEntryTrigger&) const = default;
};
struct NarrativeRegionEntryTrigger {
  bool operator==(const NarrativeRegionEntryTrigger&) const = default;
  std::string region{};
};
struct NarrativeInteractionTrigger {
  bool operator==(const NarrativeInteractionTrigger&) const = default;
  NarrativeInteractionTarget target_kind{NarrativeInteractionTarget::Door};
  std::string target{};
  NarrativeInteractionAction action{NarrativeInteractionAction::DoorInteract};
};
struct NarrativeConditionTrigger {
  bool operator==(const NarrativeConditionTrigger&) const = default;
  std::vector<NarrativePredicate> predicates{};
};
using NarrativeTrigger =
    std::variant<NarrativeSceneEntryTrigger, NarrativeRegionEntryTrigger,
                 NarrativeInteractionTrigger, NarrativeConditionTrigger>;

struct NarrativeSetFactStep {
  bool operator==(const NarrativeSetFactStep&) const = default;
  std::string fact{};
  bool value{};
};
struct NarrativeSetLightStep {
  bool operator==(const NarrativeSetLightStep&) const = default;
  std::string light{};
  bool enabled{};
};
struct NarrativeSetDoorOpenStep {
  bool operator==(const NarrativeSetDoorOpenStep&) const = default;
  std::string door{};
  bool open{};
};
struct NarrativeSetDoorLockedStep {
  bool operator==(const NarrativeSetDoorLockedStep&) const = default;
  std::string door{};
  bool locked{};
};
struct NarrativeSetRadioStep {
  bool operator==(const NarrativeSetRadioStep&) const = default;
  std::string radio{};
  bool enabled{};
};
struct NarrativePlayCueStep {
  bool operator==(const NarrativePlayCueStep&) const = default;
  std::string source{};
};
struct NarrativeRunRouteStep {
  bool operator==(const NarrativeRunRouteStep&) const = default;
  std::string actor{};
  std::string route{};
};
struct NarrativeDelayStep {
  bool operator==(const NarrativeDelayStep&) const = default;
  float seconds{};
};
struct NarrativeWaitUntilStep {
  bool operator==(const NarrativeWaitUntilStep&) const = default;
  std::vector<NarrativePredicate> predicates{};
};
using NarrativeStep = std::variant<
    NarrativeSetFactStep, NarrativeSetLightStep, NarrativeSetDoorOpenStep,
    NarrativeSetDoorLockedStep, NarrativeSetRadioStep, NarrativePlayCueStep,
    NarrativeRunRouteStep, NarrativeDelayStep, NarrativeWaitUntilStep>;

struct NarrativeFactDefinition {
  bool operator==(const NarrativeFactDefinition&) const = default;
  std::string id{};
  bool initial_value{};
};
struct NarrativeRegionDefinition {
  bool operator==(const NarrativeRegionDefinition&) const = default;
  std::string id{};
  WorldPosition center{};
  WorldExtent half_extent{1.F, 1.F, 1.F};
};
struct NarrativeEventDefinition {
  bool operator==(const NarrativeEventDefinition&) const = default;
  std::string id{};
  NarrativeTrigger trigger{};
  std::vector<NarrativePredicate> guards{};
  std::optional<std::vector<NarrativePredicate>> cancel{};
  NarrativeRepeat repeat{NarrativeRepeat::Once};
  std::vector<NarrativeStep> steps{};
};
struct LevelNarrative {
  bool operator==(const LevelNarrative&) const = default;
  std::vector<NarrativeFactDefinition> facts{};
  std::vector<NarrativeRegionDefinition> regions{};
  std::vector<NarrativeEventDefinition> events{};
};

struct LevelDocument {
  bool operator==(const LevelDocument&) const = default;
  std::uint32_t version{level_format_version};
  std::optional<PrototypeTerrain> terrain{};
  std::vector<PrototypeSolid> solids{};
  std::vector<LevelEntry> entries{};
  std::string default_entry{};
  PrototypeEnvironmentLight environment_light{};
  std::vector<PrototypeStaticProp> props{};
  std::vector<PrototypeLightSwitch> light_switches{};
  std::vector<DoorDefinition> doors{};
  LevelAudio audio{};
  LevelCharacters characters{};
  LevelHousehold household{};
  LevelNarrative narrative{};
};

enum class LevelDiagnosticCategory {
  Parse,
  Validation,
  Filesystem,
};

struct TerrainDiagnosticLocation {
  bool operator==(const TerrainDiagnosticLocation&) const = default;
  std::size_t x{};
  std::size_t z{};
  // Absent for a sample diagnostic; 0 = p00,p01,p11; 1 = p00,p11,p10.
  std::optional<unsigned> triangle{};
};

struct LevelDiagnostic {
  LevelDiagnosticCategory category{LevelDiagnosticCategory::Validation};
  std::filesystem::path source_path{};
  std::string document_path{};
  std::string message{};
  std::optional<TerrainDiagnosticLocation> terrain_location{};
};

struct LevelDocumentLoadResult {
  std::optional<LevelDocument> document{};
  std::vector<LevelDiagnostic> diagnostics{};
  std::uint32_t source_version{};

  [[nodiscard]] explicit operator bool() const noexcept {
    return document.has_value() && diagnostics.empty();
  }
};

[[nodiscard]] bool levelEntryIdIsValid(std::string_view id) noexcept;
[[nodiscard]] const LevelEntry* findLevelEntry(const LevelDocument& document,
                                               std::string_view id) noexcept;

struct LevelDocumentSaveResult {
  std::vector<LevelDiagnostic> diagnostics{};

  [[nodiscard]] explicit operator bool() const noexcept {
    return diagnostics.empty();
  }
};

// Canonical bytes are produced only after the same validation used by Save.
// Restricted editor sessions reuse this value result before touching a slot.
struct LevelDocumentSerializationResult {
  std::string bytes{};
  std::vector<LevelDiagnostic> diagnostics{};

  [[nodiscard]] explicit operator bool() const noexcept {
    return diagnostics.empty();
  }
};

[[nodiscard]] LevelDocumentSerializationResult serializeLevelDocument(
    const LevelDocument& document,
    const std::filesystem::path& diagnostic_path = {});

[[nodiscard]] std::vector<LevelDiagnostic> validateLevelDocument(
    const LevelDocument& document,
    const std::filesystem::path& source_path = {});
[[nodiscard]] LevelDocumentLoadResult loadLevelDocument(
    const std::filesystem::path& path);
[[nodiscard]] LevelDocumentSaveResult saveLevelDocument(
    const std::filesystem::path& path, const LevelDocument& document);
[[nodiscard]] std::string formatLevelDiagnostics(
    const std::vector<LevelDiagnostic>& diagnostics);

#endif
