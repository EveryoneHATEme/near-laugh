#ifndef EDITOR_EDITOR_DOCUMENT_HPP
#define EDITOR_EDITOR_DOCUMENT_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "core/world/level_document.hpp"

enum class EditorPendingActionKind { None, Open, Close, Exit };
enum class EditorPendingDecision { Save, Discard, Cancel };

struct EditorPendingAction {
  EditorPendingActionKind kind{EditorPendingActionKind::None};
  std::filesystem::path path{};
};

class EditorDocument {
 public:
  [[nodiscard]] bool open(const std::filesystem::path& path);
  [[nodiscard]] bool save();
  [[nodiscard]] bool saveAs(const std::filesystem::path& path);

  void requestOpen(const std::filesystem::path& path);
  void requestClose();
  void requestExit();
  [[nodiscard]] bool resolvePending(EditorPendingDecision decision);

  void markDirty() noexcept;
  [[nodiscard]] LevelDocument& editDocument();
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
  [[nodiscard]] bool dirty() const noexcept { return dirty_; }
  [[nodiscard]] bool valid() const noexcept {
    return document_.has_value() && validateLevelDocument(*document_).empty();
  }
  [[nodiscard]] bool exitRequested() const noexcept { return exit_requested_; }
  [[nodiscard]] const EditorPendingAction& pendingAction() const noexcept {
    return pending_;
  }
  [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

 private:
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
  bool dirty_{};
  bool exit_requested_{};
  std::uint64_t revision_{};
};

#endif
