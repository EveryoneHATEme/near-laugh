#ifndef EDITOR_EDITOR_DOCUMENT_HPP
#define EDITOR_EDITOR_DOCUMENT_HPP

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

#include "core/world/level_document.hpp"

enum class EditorPendingActionKind { None, Open, Close, Exit };
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
inline constexpr EditorObjectId editor_first_solid = 5;
using EditorObjectValue =
    std::variant<PrototypeSolid, PrototypePlayerSpawn, PrototypePointLight,
                 PrototypeStaticProp>;

// Field-level checks permit safe intermediate gameplay-invalid documents.
[[nodiscard]] std::string editorObjectFieldError(
    const EditorObjectValue& value);

class EditorDocument {
 public:
  [[nodiscard]] bool open(const std::filesystem::path& path);
  [[nodiscard]] bool save();
  [[nodiscard]] bool saveAs(const std::filesystem::path& path);

  void requestOpen(const std::filesystem::path& path);
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
  [[nodiscard]] bool duplicateSelected();
  [[nodiscard]] bool removeSelected();
  [[nodiscard]] bool placeSelected(WorldPosition terrain_hit);
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
    return current_revision_ != saved_revision_;
  }
  [[nodiscard]] bool valid() const noexcept {
    return document_.has_value() && valid_;
  }
  [[nodiscard]] bool exitRequested() const noexcept { return exit_requested_; }
  [[nodiscard]] const EditorPendingAction& pendingAction() const noexcept {
    return pending_;
  }
  [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

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
  };
  void resetEditing();
  void refreshValidation();
  [[nodiscard]] std::optional<std::size_t> solidIndex(EditorObjectId id) const;
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
  EditorObjectId next_object_id_{editor_first_solid};
  EditorObjectId selection_{};
  std::deque<Edit> history_{};
  std::size_t history_position_{};
  std::uint64_t current_revision_{};
  std::uint64_t saved_revision_{};
  std::uint64_t next_revision_{};
  std::string edit_error_{};
  bool valid_{};
  bool exit_requested_{};
  std::uint64_t revision_{};
};

#endif
