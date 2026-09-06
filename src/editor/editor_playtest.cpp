#include "editor/editor_playtest.hpp"

#include <utility>

bool EditorPlaytest::fail(std::string error) {
  cancel();
  error_ = std::move(error);
  return false;
}

void EditorPlaytest::cancel() {
  state_ = EditorPlayState::Idle;
  launch_.reset();
  prepared_.reset();
  entry_.clear();
  error_.clear();
}

bool EditorPlaytest::request(EditorDocument& document, bool child_active) {
  if (state_ != EditorPlayState::Idle) return false;
  cancel();
  static_cast<void>(document.finishTerrainStroke());
  if (child_active) return fail("A game process is already active.");
  if (!document.document()) return fail("Open or create a level before Play.");
  const auto diagnostics = validateLevelDocument(
      *document.document(), document.path().value_or(std::filesystem::path{}));
  if (!diagnostics.empty()) return fail(formatLevelDiagnostics(diagnostics));
  if (!findLevelEntry(*document.document(), document.launchEntry()))
    return fail("Select an existing playtest entry.");
  prepared_ = *document.document();
  generation_ = document.generation();
  entry_ = document.launchEntry();
  if (document.dirty() || !document.path()) {
    state_ = EditorPlayState::ConfirmSave;
    return true;
  }
  return preflight(document);
}

bool EditorPlaytest::unchanged(const EditorDocument& document) {
  if (!prepared_ || document.generation() != generation_ ||
      !document.document() || *document.document() != *prepared_ ||
      document.launchEntry() != entry_)
    return fail("The prepared level or entry changed. Start Play again.");
  return true;
}

bool EditorPlaytest::saveAndPlay(EditorDocument& document) {
  if (state_ != EditorPlayState::ConfirmSave || !unchanged(document))
    return false;
  if (!document.path()) {
    state_ = EditorPlayState::SaveAs;
    return true;
  }
  if (!document.save())
    return fail(formatLevelDiagnostics(document.diagnostics()));
  return preflight(document);
}

bool EditorPlaytest::saveAsAndPlay(EditorDocument& document,
                                   const std::filesystem::path& path) {
  if (state_ != EditorPlayState::SaveAs || !unchanged(document)) return false;
  if (path.empty()) return fail("Choose a level save path.");
  if (!document.saveAs(path))
    return fail(formatLevelDiagnostics(document.diagnostics()));
  return preflight(document);
}

bool EditorPlaytest::preflight(const EditorDocument& document) {
  if (!unchanged(document)) return false;
  if (!document.path()) return fail("Save the level before Play.");
  const auto loaded = loadLevelDocument(*document.path());
  if (!loaded) return fail(formatLevelDiagnostics(loaded.diagnostics));
  const auto diagnostics =
      validateLevelDocument(*loaded.document, *document.path());
  if (!diagnostics.empty()) return fail(formatLevelDiagnostics(diagnostics));
  if (!findLevelEntry(*loaded.document, entry_))
    return fail("Saved level does not contain the chosen entry.");
  if (*loaded.document != *prepared_)
    return fail(
        "The saved level differs from the editor. Explicitly Save or Open it "
        "before Play.");
  launch_ = EditorLaunchRequest{*document.path(), entry_};
  state_ = EditorPlayState::Ready;
  return true;
}

std::optional<EditorLaunchRequest> EditorPlaytest::consume() {
  if (state_ != EditorPlayState::Ready) return std::nullopt;
  auto request = std::move(launch_);
  cancel();
  return request;
}
