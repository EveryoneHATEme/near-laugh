#ifndef EDITOR_EDITOR_DOCUMENT_HPP
#define EDITOR_EDITOR_DOCUMENT_HPP

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

#include "core/world/level_document.hpp"
#include "editor/editor_terrain.hpp"

enum class EditorPendingActionKind { None, Open, NewInterior, Close, Exit };
enum class EditorPendingDecision { Save, Discard, Cancel };

struct EditorPendingAction {
  EditorPendingActionKind kind{EditorPendingActionKind::None};
  std::filesystem::path path{};
};

using EditorObjectId = std::uint64_t;
inline constexpr EditorObjectId editor_no_object = 0;
inline constexpr EditorObjectId editor_spawn = 1;
inline constexpr EditorObjectId editor_first_light = 2;
inline constexpr EditorObjectId editor_prop = 4;
inline constexpr EditorObjectId editor_light_switch = 5;
inline constexpr EditorObjectId editor_first_solid = 6;
using EditorObjectValue =
    std::variant<PrototypeSolid, LevelEntry, PrototypePointLight,
                 PrototypeStaticProp, PrototypeLightSwitch, DoorDefinition>;

enum class EditorPlacementMode { SceneSurfaces, TerrainOnly };
enum class EditorSurfaceFace {
  Terrain,
  Top,
  Bottom,
  NegativeX,
  PositiveX,
  NegativeZ,
  PositiveZ
};
struct EditorSurfaceHit {
  WorldPosition position{};
  WorldPosition normal{};
  double distance{};
  EditorObjectId target{};
  EditorSurfaceFace face{EditorSurfaceFace::Terrain};
};
struct EditorPlacementOffsets {
  float height{2.0F};
  float outward{0.1F};
  float door_clearance{0.02F};
};
[[nodiscard]] std::optional<EditorObjectValue> editorPlacedObject(
    EditorObjectValue value, const EditorSurfaceHit& hit,
    const EditorPlacementOffsets& offsets);

// Field-level checks permit safe intermediate gameplay-invalid documents.
[[nodiscard]] std::string editorObjectFieldError(
    const EditorObjectValue& value);

class EditorDocument {
 public:
  [[nodiscard]] bool open(const std::filesystem::path& path);
  [[nodiscard]] bool save();
  [[nodiscard]] bool saveAs(const std::filesystem::path& path);

  void requestOpen(const std::filesystem::path& path);
  void requestNewInterior();
  void requestClose();
  void requestExit();
  [[nodiscard]] bool resolvePending(EditorPendingDecision decision);

  [[nodiscard]] std::optional<EditorObjectValue> object(
      EditorObjectId id) const;
  [[nodiscard]] const std::vector<EditorObjectId>& solidIds() const noexcept {
    return solid_ids_;
  }
  [[nodiscard]] EditorObjectId selection() const noexcept { return selection_; }
  void select(EditorObjectId id);
  [[nodiscard]] bool replaceObject(EditorObjectId id, EditorObjectValue value);
  [[nodiscard]] bool addSolid(PrototypeSolid solid);
  [[nodiscard]] bool addLightSwitch();
  [[nodiscard]] bool addDoor();
  [[nodiscard]] bool addProp(std::string_view model);
  [[nodiscard]] bool setTerrainMaterial(std::string material);
  [[nodiscard]] const std::vector<EditorObjectId>& doorIds() const noexcept {
    return door_ids_;
  }
  [[nodiscard]] const std::vector<EditorObjectId>& propIds() const noexcept {
    return prop_ids_;
  }
  [[nodiscard]] bool addEntry(PrototypePlayerSpawn pose);
  [[nodiscard]] bool makeSelectedEntryDefault();
  [[nodiscard]] const std::vector<EditorObjectId>& entryIds() const noexcept {
    return entry_ids_;
  }
  [[nodiscard]] const std::string& launchEntry() const noexcept {
    return launch_entry_;
  }
  [[nodiscard]] bool selectLaunchEntry(std::string_view id);
  [[nodiscard]] std::uint32_t sourceVersion() const noexcept {
    return source_version_;
  }
  [[nodiscard]] bool duplicateSelected();
  [[nodiscard]] bool removeSelected();
  [[nodiscard]] bool placeSelected(WorldPosition terrain_hit);
  [[nodiscard]] const EditorTerrainBrush& terrainBrush() const noexcept {
    return terrain_stroke_ ? terrain_stroke_->brush : terrain_brush_;
  }
  [[nodiscard]] bool setTerrainBrush(const EditorTerrainBrush& brush);
  void beginTerrainStroke(std::optional<WorldPosition> hit);
  void extendTerrainStroke(std::optional<WorldPosition> hit);
  [[nodiscard]] bool finishTerrainStroke();
  [[nodiscard]] bool terrainStrokeActive() const noexcept {
    return terrain_stroke_.has_value();
  }
  [[nodiscard]] bool canUndo() const noexcept { return history_position_ != 0; }
  [[nodiscard]] bool canRedo() const noexcept {
    return history_position_ < history_.size();
  }
  [[nodiscard]] bool undo();
  [[nodiscard]] bool redo();
  [[nodiscard]] const std::string& editError() const noexcept {
    return edit_error_;
  }
  void reportResourceError(std::string message);

  [[nodiscard]] const std::optional<LevelDocument>& document() const noexcept {
    return document_;
  }
  [[nodiscard]] const std::optional<std::filesystem::path>& path()
      const noexcept {
    return path_;
  }
  [[nodiscard]] const std::vector<LevelDiagnostic>& diagnostics()
      const noexcept {
    return diagnostics_;
  }
  [[nodiscard]] bool dirty() const noexcept {
    return current_revision_ != saved_revision_ ||
           (terrain_stroke_ && !terrain_stroke_->before.empty());
  }
  [[nodiscard]] bool valid() const noexcept {
    return document_.has_value() && valid_ && !terrain_stroke_;
  }
  [[nodiscard]] bool exitRequested() const noexcept { return exit_requested_; }
  [[nodiscard]] const EditorPendingAction& pendingAction() const noexcept {
    return pending_;
  }
  [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
  [[nodiscard]] std::uint64_t objectRevision() const noexcept {
    return object_revision_;
  }
  [[nodiscard]] std::uint64_t generation() const noexcept {
    return generation_;
  }

 private:
  struct Edit {
    EditorObjectId id{};
    std::size_t index{};
    std::optional<EditorObjectValue> before{};
    std::optional<EditorObjectValue> after{};
    EditorObjectId selection_before{};
    EditorObjectId selection_after{};
    std::uint64_t revision_before{};
    std::uint64_t revision_after{};
    std::vector<EditorTerrainSampleEdit> terrain{};
    std::optional<EditorTerrainBrush> brush{};
    std::optional<std::string> default_before{};
    std::optional<std::string> default_after{};
    std::optional<std::string> terrain_material_before{};
    std::optional<std::string> terrain_material_after{};
  };
  void resetEditing();
  void refreshValidation();
  [[nodiscard]] std::optional<std::size_t> solidIndex(EditorObjectId id) const;
  [[nodiscard]] std::optional<std::size_t> entryIndex(EditorObjectId id) const;
  [[nodiscard]] std::optional<std::size_t> doorIndex(EditorObjectId id) const;
  [[nodiscard]] std::optional<std::size_t> propIndex(EditorObjectId id) const;
  void newInterior();
  [[nodiscard]] bool commit(Edit edit);
  void applyEdit(const Edit& edit, bool forward);
  void performClose() noexcept;
  void performExit() noexcept;
  [[nodiscard]] bool performPendingAction();
  void setOperationError(LevelDiagnosticCategory category,
                         const std::filesystem::path& path,
                         std::string message);

  std::optional<LevelDocument> document_{};
  std::optional<std::filesystem::path> path_{};
  std::vector<LevelDiagnostic> diagnostics_{};
  EditorPendingAction pending_{};
  std::vector<EditorObjectId> solid_ids_{};
  std::vector<EditorObjectId> entry_ids_{};
  std::vector<EditorObjectId> door_ids_{};
  std::vector<EditorObjectId> prop_ids_{};
  std::string launch_entry_{};
  std::uint32_t source_version_{level_format_version};
  EditorObjectId next_object_id_{editor_first_solid};
  EditorObjectId selection_{};
  std::deque<Edit> history_{};
  std::size_t history_position_{};
  std::uint64_t current_revision_{};
  std::uint64_t saved_revision_{};
  std::uint64_t next_revision_{};
  std::string edit_error_{};
  EditorTerrainBrush terrain_brush_{};
  std::optional<EditorTerrainStroke> terrain_stroke_{};
  EditorObjectId stroke_selection_{};
  bool valid_{};
  bool exit_requested_{};
  std::uint64_t revision_{};
  std::uint64_t object_revision_{};
  std::uint64_t generation_{};
};

#endif
