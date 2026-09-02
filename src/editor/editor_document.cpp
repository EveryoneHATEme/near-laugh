#include "editor/editor_document.hpp"

#include <stdexcept>
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
  path_ = candidate_path;
  diagnostics_.clear();
  dirty_ = false;
  ++revision_;
  return true;
}

bool EditorDocument::save() {
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
  dirty_ = false;
  return true;
}

bool EditorDocument::saveAs(const std::filesystem::path& path) {
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
  dirty_ = false;
  return true;
}

void EditorDocument::requestOpen(const std::filesystem::path& path) {
  if (dirty_) {
    pending_ = {EditorPendingActionKind::Open, resolvedPath(path)};
    return;
  }
  static_cast<void>(open(path));
}

void EditorDocument::requestClose() {
  if (dirty_) {
    pending_ = {EditorPendingActionKind::Close, {}};
    return;
  }
  performClose();
}

void EditorDocument::requestExit() {
  if (dirty_) {
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

void EditorDocument::markDirty() noexcept {
  if (document_) {
    dirty_ = true;
  }
}

LevelDocument& EditorDocument::editDocument() {
  if (!document_) {
    throw std::logic_error("Cannot edit without an open level document");
  }
  dirty_ = true;
  return *document_;
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
    dirty_ = false;
    ++revision_;
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
