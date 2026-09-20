#include "editor/editor_document.hpp"

#include <algorithm>
#include <string>
#include <utility>

EditorDocument::EditorDocument(std::shared_ptr<EditorFilePolicy> file_policy)
    : file_policy_(std::move(file_policy)) {}

bool EditorDocument::open(const std::filesystem::path& path) {
  auto permit = filePermit(path, EditorFilePolicy::Access::Read);
  if (file_policy_ && !permit) return false;
  const auto candidate_path = permit ? std::optional(permit->path())
                                     : resolvePath(path);
  if (!candidate_path) {
    return false;
  }
  LevelDocumentLoadResult candidate = loadLevelDocument(*candidate_path);
  if (!candidate) {
    diagnostics_ = std::move(candidate.diagnostics);
    return false;
  }

  document_ = std::move(candidate.document);
  source_version_ = candidate.source_version;
  path_ = candidate_path;
  diagnostics_.clear();
  resetEditing();
  return true;
}

bool EditorDocument::save() {
  auto permit = path_ ? filePermit(*path_, EditorFilePolicy::Access::Write)
                      : std::nullopt;
  if (file_policy_ && path_ && !permit) return false;
  static_cast<void>(finishTerrainStroke());
  if (!document_) {
    setOperationError(LevelDiagnosticCategory::Validation, {},
                      "No level document is open");
    return false;
  }
  if (!path_) {
    setOperationError(LevelDiagnosticCategory::Filesystem, {},
                      "The open level has no save path; use Save As");
    return false;
  }

  const LevelDocumentSaveResult result = file_policy_
      ? file_policy_->save(*permit, *document_)
      : saveLevelDocument(*path_, *document_);
  if (!result) {
    if (file_policy_ && std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [](const auto& diagnostic) {
              return diagnostic.message.starts_with("policy_denied:");
            }))
      ++policy_denial_revision_;
    diagnostics_ = result.diagnostics;
    return false;
  }
  diagnostics_.clear();
  saved_revision_ = current_revision_;
  source_version_ = level_format_version;
  return true;
}

bool EditorDocument::saveAs(const std::filesystem::path& path) {
  auto permit = filePermit(path, EditorFilePolicy::Access::Write);
  if (file_policy_ && !permit) return false;
  static_cast<void>(finishTerrainStroke());
  if (!document_) {
    setOperationError(LevelDiagnosticCategory::Validation, path,
                      "No level document is open");
    return false;
  }
  const auto candidate_path = permit ? std::optional(permit->path())
                                     : resolvePath(path);
  if (!candidate_path) {
    return false;
  }
  const LevelDocumentSaveResult result =
      file_policy_ ? file_policy_->save(*permit, *document_)
                   : saveLevelDocument(*candidate_path, *document_);
  if (!result) {
    if (file_policy_ && std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [](const auto& diagnostic) {
              return diagnostic.message.starts_with("policy_denied:");
            }))
      ++policy_denial_revision_;
    diagnostics_ = result.diagnostics;
    return false;
  }
  path_ = candidate_path;
  diagnostics_.clear();
  saved_revision_ = current_revision_;
  source_version_ = level_format_version;
  return true;
}

void EditorDocument::requestOpen(const std::filesystem::path& path) {
  auto permit = filePermit(path, EditorFilePolicy::Access::Read);
  if (file_policy_ && !permit) return;
  static_cast<void>(finishTerrainStroke());
  if (dirty()) {
    const auto candidate_path = permit ? std::optional(permit->path())
                                       : resolvePath(path);
    if (candidate_path) {
      pending_ = {EditorPendingActionKind::Open, *candidate_path};
    }
    return;
  }
  static_cast<void>(open(path));
}

void EditorDocument::requestNewInterior() {
  static_cast<void>(finishTerrainStroke());
  if (dirty()) {
    pending_ = {EditorPendingActionKind::NewInterior, {}};
    return;
  }
  newInterior();
}

void EditorDocument::newInterior() {
  LevelDocument interior;
  interior.solids = {{{0, -0.25F, 0},
                      {5, 0.25F, 5},
                      {150, 155, 165, 255},
                      PrototypeSolidKind::Floor,
                      "prototype-floor"}};
  interior.entries = {{"default", {{0, 0, 2}, -90}}};
  interior.default_entry = "default";
  interior.environment_light = {
      {{{0, 2.4F, 2}, {0.3F, 0.5F, 0.9F}, 0.65F, 5, "point-light-0"},
       {{0, 2.8F, -2}, {1, 0.48F, 0.2F}, 0.95F, 6, "point-light-1"}},
      0.12F};
  document_ = std::move(interior);
  source_version_ = level_format_version;
  path_.reset();
  resetEditing();
  saved_revision_ = 0;
}

void EditorDocument::requestClose() {
  static_cast<void>(finishTerrainStroke());
  if (dirty()) {
    pending_ = {EditorPendingActionKind::Close, {}};
    return;
  }
  performClose();
}

void EditorDocument::requestExit() {
  static_cast<void>(finishTerrainStroke());
  if (dirty()) {
    pending_ = {EditorPendingActionKind::Exit, {}};
    return;
  }
  performExit();
}

bool EditorDocument::resolvePending(EditorPendingDecision decision) {
  if (pending_.kind == EditorPendingActionKind::None) {
    return false;
  }
  if (decision == EditorPendingDecision::Cancel) {
    pending_ = {};
    return true;
  }
  if (file_policy_ && pending_.kind == EditorPendingActionKind::Open) {
    // Validate the pending target again before Save changes the active file.
    // Release this read pin before saving, since Open may name that same file.
    auto permit = filePermit(pending_.path, EditorFilePolicy::Access::Read);
    if (!permit) return false;
  }
  if (decision == EditorPendingDecision::Save && !save()) {
    return false;
  }
  return performPendingAction();
}

void EditorDocument::reportResourceError(std::string message) {
  setOperationError(LevelDiagnosticCategory::Filesystem,
                    path_.value_or(std::filesystem::path{}),
                    std::move(message));
}

void EditorDocument::performClose() noexcept {
  if (document_) {
    document_.reset();
    path_.reset();
    diagnostics_.clear();
    resetEditing();
  }
}

void EditorDocument::performExit() noexcept { exit_requested_ = true; }

bool EditorDocument::performPendingAction() {
  const EditorPendingAction action = pending_;
  // Restricted Open preserves its decision on policy/load failure. A successful
  // open resets pending state together with the replaced document.
  if (file_policy_ && action.kind == EditorPendingActionKind::Open)
    return open(action.path);
  pending_ = {};
  switch (action.kind) {
    case EditorPendingActionKind::None:
      return false;
    case EditorPendingActionKind::Open:
      return open(action.path);
    case EditorPendingActionKind::NewInterior:
      newInterior();
      return true;
    case EditorPendingActionKind::Close:
      performClose();
      return true;
    case EditorPendingActionKind::Exit:
      performExit();
      return true;
  }
  return false;
}

std::optional<EditorFilePolicy::Permit> EditorDocument::filePermit(
    const std::filesystem::path& path, EditorFilePolicy::Access access) {
  if (!file_policy_) return std::nullopt;
  try {
    return file_policy_->permit(path, access);
  } catch (const std::exception& error) {
    setOperationError(LevelDiagnosticCategory::Filesystem, path, error.what());
    return std::nullopt;
  }
}

std::optional<std::filesystem::path> EditorDocument::resolvePath(
    const std::filesystem::path& path) {
  if (path.empty()) {
    setOperationError(LevelDiagnosticCategory::Filesystem, path,
                      "Choose a level file path");
    return std::nullopt;
  }
  std::error_code error;
  auto absolute_path = std::filesystem::absolute(path, error);
  if (error) {
    setOperationError(LevelDiagnosticCategory::Filesystem, path,
                      "Cannot resolve the level file path: " + error.message());
    return std::nullopt;
  }
  return absolute_path.lexically_normal();
}

void EditorDocument::reportPolicyDenial(std::string message) {
  if (!file_policy_) return;
  if (!message.starts_with("policy_denied:"))
    message = "policy_denied: " + message;
  setOperationError(LevelDiagnosticCategory::Filesystem,
                    path_.value_or(std::filesystem::path{}), std::move(message));
}

void EditorDocument::setOperationError(LevelDiagnosticCategory category,
                                       const std::filesystem::path& path,
                                       std::string message) {
  if (file_policy_ && message.starts_with("policy_denied:"))
    ++policy_denial_revision_;
  diagnostics_ = {{category, path, {}, std::move(message)}};
}
