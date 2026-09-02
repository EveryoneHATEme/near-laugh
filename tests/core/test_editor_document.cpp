#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "editor/editor_document.hpp"

namespace {
std::filesystem::path packagedLevel() {
  return std::filesystem::absolute("resources/levels/prototype.level.json")
      .lexically_normal();
}

std::filesystem::path freshTemporaryRoot(std::string_view name) {
  const std::filesystem::path root =
      (std::filesystem::temp_directory_path() / name).lexically_normal();
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  return root;
}
}  // namespace

TEST(EditorDocument, OpenIsTransactionalAndReportsRejectedPath) {
  EditorDocument document;
  ASSERT_TRUE(document.open(packagedLevel()));
  const std::filesystem::path original_path = *document.path();
  const std::size_t original_solids = document.document()->solids.size();
  document.markDirty();

  const std::filesystem::path missing =
      packagedLevel().parent_path() / "missing.level.json";
  EXPECT_FALSE(document.open(missing));
  ASSERT_TRUE(document.document());
  EXPECT_EQ(document.document()->solids.size(), original_solids);
  EXPECT_EQ(document.path(), original_path);
  EXPECT_TRUE(document.dirty());
  ASSERT_FALSE(document.diagnostics().empty());
  EXPECT_EQ(document.diagnostics().front().source_path, missing);
}

TEST(EditorDocument, SaveAsUpdatesPathAndSaveFailurePreservesState) {
  const std::filesystem::path root =
      freshTemporaryRoot("near_laugh_editor_document_save");
  EditorDocument document;
  ASSERT_TRUE(document.open(packagedLevel()));
  document.markDirty();
  const std::filesystem::path saved = root / "saved.level.json";
  ASSERT_TRUE(document.saveAs(saved));
  EXPECT_EQ(document.path(),
            std::filesystem::absolute(saved).lexically_normal());
  EXPECT_FALSE(document.dirty());
  EXPECT_TRUE(std::filesystem::is_regular_file(saved));
  EXPECT_TRUE(loadLevelDocument(saved));

  document.editDocument().solids[0].half_extent.x = 0.0F;
  const std::filesystem::path prior_path = *document.path();
  EXPECT_FALSE(document.save());
  EXPECT_EQ(document.path(), prior_path);
  EXPECT_TRUE(document.dirty());
  ASSERT_FALSE(document.diagnostics().empty());
  EXPECT_EQ(document.diagnostics().front().category,
            LevelDiagnosticCategory::Validation);

  const std::filesystem::path missing_parent = root / "missing" / "save.json";
  EXPECT_FALSE(document.saveAs(missing_parent));
  EXPECT_EQ(document.path(), prior_path);
  EXPECT_TRUE(document.dirty());
  std::filesystem::remove_all(root);
}

TEST(EditorDocument, CleanActionsExecuteWithoutPrompt) {
  EditorDocument close_document;
  ASSERT_TRUE(close_document.open(packagedLevel()));
  close_document.requestClose();
  EXPECT_FALSE(close_document.document());
  EXPECT_EQ(close_document.pendingAction().kind, EditorPendingActionKind::None);

  EditorDocument exit_document;
  exit_document.requestExit();
  EXPECT_TRUE(exit_document.exitRequested());
  EXPECT_EQ(exit_document.pendingAction().kind, EditorPendingActionKind::None);

  EditorDocument open_document;
  open_document.requestOpen(packagedLevel());
  EXPECT_TRUE(open_document.document());
  EXPECT_FALSE(open_document.dirty());
}

TEST(EditorDocument, DirtyCloseSupportsSaveDiscardAndCancel) {
  const auto make_dirty = [] {
    EditorDocument document;
    EXPECT_TRUE(document.open(packagedLevel()));
    document.markDirty();
    document.requestClose();
    return document;
  };

  EditorDocument canceled = make_dirty();
  EXPECT_TRUE(canceled.resolvePending(EditorPendingDecision::Cancel));
  EXPECT_TRUE(canceled.document());
  EXPECT_TRUE(canceled.dirty());

  EditorDocument discarded = make_dirty();
  EXPECT_TRUE(discarded.resolvePending(EditorPendingDecision::Discard));
  EXPECT_FALSE(discarded.document());

  EditorDocument saved = make_dirty();
  EXPECT_TRUE(saved.resolvePending(EditorPendingDecision::Save));
  EXPECT_FALSE(saved.document());
}

TEST(EditorDocument, DirtyExitSupportsSaveDiscardAndCancel) {
  const auto make_dirty = [] {
    EditorDocument document;
    EXPECT_TRUE(document.open(packagedLevel()));
    document.markDirty();
    document.requestExit();
    return document;
  };

  EditorDocument canceled = make_dirty();
  EXPECT_TRUE(canceled.resolvePending(EditorPendingDecision::Cancel));
  EXPECT_FALSE(canceled.exitRequested());
  EXPECT_TRUE(canceled.document());

  EditorDocument discarded = make_dirty();
  EXPECT_TRUE(discarded.resolvePending(EditorPendingDecision::Discard));
  EXPECT_TRUE(discarded.exitRequested());

  EditorDocument saved = make_dirty();
  EXPECT_TRUE(saved.resolvePending(EditorPendingDecision::Save));
  EXPECT_TRUE(saved.exitRequested());
}

TEST(EditorDocument, DirtyOpenSupportsSaveDiscardCancelAndFailedCandidate) {
  const std::filesystem::path root =
      freshTemporaryRoot("near_laugh_editor_document_open");
  const std::filesystem::path candidate = root / "candidate.level.json";
  ASSERT_TRUE(saveLevelDocument(candidate,
                                *loadLevelDocument(packagedLevel()).document));

  const auto make_dirty = [&] {
    EditorDocument document;
    EXPECT_TRUE(document.open(packagedLevel()));
    document.markDirty();
    document.requestOpen(candidate);
    return document;
  };

  EditorDocument canceled = make_dirty();
  EXPECT_TRUE(canceled.resolvePending(EditorPendingDecision::Cancel));
  EXPECT_EQ(canceled.path(), packagedLevel());
  EXPECT_TRUE(canceled.dirty());

  EditorDocument discarded = make_dirty();
  EXPECT_TRUE(discarded.resolvePending(EditorPendingDecision::Discard));
  EXPECT_EQ(discarded.path(),
            std::filesystem::absolute(candidate).lexically_normal());
  EXPECT_FALSE(discarded.dirty());

  EditorDocument saved = make_dirty();
  EXPECT_TRUE(saved.resolvePending(EditorPendingDecision::Save));
  EXPECT_EQ(saved.path(),
            std::filesystem::absolute(candidate).lexically_normal());
  EXPECT_FALSE(saved.dirty());

  EditorDocument rejected;
  ASSERT_TRUE(rejected.open(packagedLevel()));
  rejected.markDirty();
  rejected.requestOpen(root / "missing.level.json");
  EXPECT_FALSE(rejected.resolvePending(EditorPendingDecision::Discard));
  EXPECT_EQ(rejected.path(), packagedLevel());
  EXPECT_TRUE(rejected.dirty());

  std::filesystem::remove_all(root);
}

TEST(EditorDocument, FailedPendingSaveRetainsActionAndDocument) {
  EditorDocument document;
  ASSERT_TRUE(document.open(packagedLevel()));
  document.editDocument().solids[0].half_extent.x = 0.0F;
  document.requestExit();
  EXPECT_FALSE(document.resolvePending(EditorPendingDecision::Save));
  EXPECT_EQ(document.pendingAction().kind, EditorPendingActionKind::Exit);
  EXPECT_FALSE(document.exitRequested());
  EXPECT_TRUE(document.document());
  EXPECT_TRUE(document.dirty());
}
