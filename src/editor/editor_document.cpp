#include "editor/editor_document.hpp"

#include <string>
#include <utility>

namespace {
std::filesystem::path resolvedPath(const std::filesystem::path& path) {
  return std::filesystem::absolute(path).lexically_normal();
}
}  // namespace

bool EditorDocument::open(const std::filesystem::path& path) {
  const std::filesystem::path candidate_path = resolvedPath(path);
  LevelDocumentLoadResult candidate = loadLevelDocument(candidate_path);
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

  const LevelDocumentSaveResult result = saveLevelDocument(*path_, *document_);
  if (!result) {
    diagnostics_ = result.diagnostics;
    return false;
  }
  diagnostics_.clear();
  saved_revision_ = current_revision_;
  source_version_ = level_format_version;
  return true;
}

bool EditorDocument::saveAs(const std::filesystem::path& path) {
  static_cast<void>(finishTerrainStroke());
  if (!document_) {
    setOperationError(LevelDiagnosticCategory::Validation, path,
                      "No level document is open");
    return false;
  }
  const std::filesystem::path candidate_path = resolvedPath(path);
  const LevelDocumentSaveResult result =
      saveLevelDocument(candidate_path, *document_);
  if (!result) {
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
  static_cast<void>(finishTerrainStroke());
  if (dirty()) {
    pending_ = {EditorPendingActionKind::Open, resolvedPath(path)};
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
                      PrototypeSurface::Floor}};
  interior.entries = {{"default", {{0, 0, 2}, -90}}};
  interior.default_entry = "default";
  interior.environment_light = {{{{{0, 2.4F, 2}, {0.3F, 0.5F, 0.9F}, 0.65F, 5},
                                  {{0, 2.8F, -2}, {1, 0.48F, 0.2F}, 0.95F, 6}}},
                                0.12F};
  interior.static_prop = {
      {3, 0, -2},           -25, 1, PrototypeSurface::Obstacle, {0, 0.91F, 0},
      {0.55F, 0.91F, 0.48F}};
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

void EditorDocument::setOperationError(LevelDiagnosticCategory category,
                                       const std::filesystem::path& path,
                                       std::string message) {
  diagnostics_ = {{category, path, {}, std::move(message)}};
}
