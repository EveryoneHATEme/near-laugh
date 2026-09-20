#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "editor/editor_document.hpp"
#include "editor/editor_file_policy.hpp"
#include "editor/editor_playtest.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winioctl.h>
#endif

namespace {
class TemporaryRoot {
 public:
  TemporaryRoot()
      : path(std::filesystem::temp_directory_path() /
             ("near-laugh-policy-" +
              std::to_string(std::chrono::steady_clock::now()
                                 .time_since_epoch().count()))) {
    std::filesystem::create_directory(path);
  }
  ~TemporaryRoot() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
  std::filesystem::path path;
};

void makeFixture(const std::filesystem::path& path) {
  EditorDocument document;
  document.requestNewInterior();
  if (!document.saveAs(path))
    throw std::runtime_error(formatLevelDiagnostics(document.diagnostics()));
}

void editYaw(EditorDocument& document) {
  auto entry = document.document()->entries.front();
  entry.pose.yaw_degrees += 1;
  ASSERT_TRUE(document.replaceObject(document.entryIds().front(), entry));
}

std::string readBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

TEST(EditorFilePolicy, OrdinaryEditorRetainsUnrestrictedSaveAndRead) {
  TemporaryRoot root;
  EditorDocument document;
  EXPECT_EQ(document.filePolicy(), nullptr);
  document.requestNewInterior();
  ASSERT_TRUE(document.saveAs(root.path / "ordinary.json"));
  editYaw(document);
  ASSERT_TRUE(document.save());
  EXPECT_FALSE(document.dirty());
  ASSERT_TRUE(document.open(root.path / "ordinary.json"));
  EditorGameProcess process;
  EXPECT_FALSE(process.start({}, {}));
  EXPECT_EQ(process.status().find("policy_denied"), std::string::npos);
}

#if defined(_WIN32)
std::error_code createJunction(const std::filesystem::path& path,
                               const std::filesystem::path& target,
                               bool create_directory = true) {
  struct MountPoint {
    DWORD tag;
    WORD bytes, reserved;
    WORD substitute_offset, substitute_bytes, print_offset, print_bytes;
    wchar_t paths[1];
  };
  const auto substitute = L"\\??\\" + target.native();
  const auto& print = target.native();
  const auto path_bytes =
      (substitute.size() + print.size() + 2) * sizeof(wchar_t);
  std::vector<std::byte> storage(offsetof(MountPoint, paths) + path_bytes);
  auto* data = reinterpret_cast<MountPoint*>(storage.data());
  data->tag = IO_REPARSE_TAG_MOUNT_POINT;
  data->bytes = static_cast<WORD>(storage.size() - 8);
  data->substitute_bytes = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
  data->print_offset = static_cast<WORD>((substitute.size() + 1) * sizeof(wchar_t));
  data->print_bytes = static_cast<WORD>(print.size() * sizeof(wchar_t));
  std::copy(substitute.begin(), substitute.end(), data->paths);
  std::copy(print.begin(), print.end(), data->paths + substitute.size() + 1);
  if (create_directory && !CreateDirectoryW(path.c_str(), nullptr))
    return {static_cast<int>(GetLastError()), std::system_category()};
  HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE |
                                   FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT |
                                                  FILE_FLAG_BACKUP_SEMANTICS,
                               nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    return {static_cast<int>(GetLastError()), std::system_category()};
  DWORD returned{};
  const bool created = DeviceIoControl(
      handle, FSCTL_SET_REPARSE_POINT, data, static_cast<DWORD>(storage.size()),
      nullptr, 0, &returned, nullptr) != FALSE;
  const DWORD error = created ? ERROR_SUCCESS : GetLastError();
  CloseHandle(handle);
  return {static_cast<int>(error), std::system_category()};
}

class RestrictedEditorFiles : public testing::Test {
 protected:
  void SetUp() override {
    makeFixture(root.path / "input.level.json");
    makeFixture(root.path / "other.level.json");
    policy = std::make_shared<EditorFilePolicy>(
        root.path,
        std::vector<std::filesystem::path>{"input.level.json", "other.level.json"},
        std::vector<std::filesystem::path>{"output.level.json"});
  }
  TemporaryRoot root;
  std::shared_ptr<EditorFilePolicy> policy;
};

TEST_F(RestrictedEditorFiles, SavesAndReopensOwnedSlotsWithExclusiveWrites) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  editYaw(document);
  ASSERT_TRUE(document.save()) << formatLevelDiagnostics(document.diagnostics());
  const auto saved = *document.document();
  ASSERT_TRUE(document.saveAs(root.path / "output.level.json"));
  EXPECT_FALSE(document.dirty());
  editYaw(document);
  ASSERT_TRUE(document.save());
  ASSERT_TRUE(document.open("input.level.json"));
  EXPECT_EQ(*document.document(), saved);
  ASSERT_TRUE(document.open("output.level.json"));
  EXPECT_NE(*document.document(), saved);
}

TEST_F(RestrictedEditorFiles, RawUnsafePathsCannotBeNormalizedIntoAnAllowedSlot) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  editYaw(document);
  const auto before = *document.document();
  const auto before_path = document.path();
  const std::vector<std::filesystem::path> rejected{
      "../input.level.json", "sub/../input.level.json", "./input.level.json",
      root.path / ".." / root.path.filename() / "input.level.json",
      root.path.parent_path() / "outside.level.json",
      "unknown.level.json", "input.level.json:stream", "input.level.json.",
      "input.level.json ", "CON.level.json", "C:input.level.json",
      R"(\\server\share\input.level.json)", R"(\\?\C:\input.level.json)",
      R"(\\.\C:\input.level.json)"};
  for (const auto& path : rejected) {
    EXPECT_FALSE(document.open(path));
    EXPECT_FALSE(document.saveAs(path));
    document.requestOpen(path);
    EXPECT_EQ(document.pendingAction().kind, EditorPendingActionKind::None);
    EXPECT_EQ(*document.document(), before);
    EXPECT_EQ(document.path(), before_path);
    EXPECT_TRUE(document.dirty());
    EXPECT_NE(formatLevelDiagnostics(document.diagnostics()).find("policy_denied"),
              std::string::npos);
  }
}

TEST_F(RestrictedEditorFiles, UnownedOutputCannotBeOverwritten) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  editYaw(document);
  const auto output = root.path / "output.level.json";
  { std::ofstream file(output); file << "not owned"; }
  EXPECT_FALSE(document.saveAs(output));
  EXPECT_EQ(readBytes(output), "not owned");
  EXPECT_TRUE(document.dirty());
  EXPECT_EQ(document.path(), root.path / "input.level.json");
}

TEST_F(RestrictedEditorFiles, OutputInsertedAfterWritePermitIsNeverOverwrittenOrAdopted) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  auto permit = policy->permit("output.level.json", EditorFilePolicy::Access::Write);
  const auto output = root.path / "output.level.json";
  { std::ofstream foreign(output); foreign << "foreign insertion after permit"; }
  const auto result = policy->save(permit, *document.document());
  EXPECT_FALSE(result);
  EXPECT_NE(formatLevelDiagnostics(result.diagnostics).find("policy_denied"),
            std::string::npos);
  EXPECT_EQ(readBytes(output), "foreign insertion after permit");
  EXPECT_THROW(static_cast<void>(policy->permit(output, EditorFilePolicy::Access::Read)),
               std::runtime_error);
}

TEST_F(RestrictedEditorFiles, ExistingWritePermitPreventsConcurrentReplacementAndModification) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  const auto input = root.path / "input.level.json";
  {
    auto permit = policy->permit(input, EditorFilePolicy::Access::Write);
    { std::ofstream overwrite(input, std::ios::trunc); EXPECT_FALSE(overwrite.is_open()); }
    std::error_code error;
    std::filesystem::rename(input, root.path / "foreign-move", error);
    EXPECT_TRUE(error);
    EXPECT_TRUE(policy->save(permit, *document.document()));
    error.clear();
    std::filesystem::remove(input, error);
    EXPECT_TRUE(error);
  }
  EXPECT_TRUE(document.open(input));
}

TEST_F(RestrictedEditorFiles, HardlinkInsertedAfterPermitIsRejectedBeforeWriting) {
  TemporaryRoot outside;
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  const auto input = root.path / "input.level.json";
  const auto original = readBytes(input);
  editYaw(document);
  {
    auto permit = policy->permit(input, EditorFilePolicy::Access::Write);
    // Windows permits adding an alias despite the source's exclusive handle.
    // The write entry must recheck link count before changing any bytes.
    std::filesystem::create_hard_link(input, outside.path / "alias");
    const auto result = policy->save(permit, *document.document());
    EXPECT_FALSE(result);
    EXPECT_NE(formatLevelDiagnostics(result.diagnostics).find("policy_denied"),
              std::string::npos);
  }
  EXPECT_EQ(readBytes(input), original);
  EXPECT_EQ(readBytes(outside.path / "alias"), original);
}

TEST_F(RestrictedEditorFiles, DenialRevisionTracksOnlyNewRestrictedAttempts) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  EXPECT_EQ(document.policyDenialRevision(), 0U);
  EXPECT_FALSE(document.open("../outside.level.json"));
  EXPECT_EQ(document.policyDenialRevision(), 1U);
  editYaw(document);
  EXPECT_EQ(document.policyDenialRevision(), 1U);
  EXPECT_FALSE(document.saveAs("../outside.level.json"));
  EXPECT_EQ(document.policyDenialRevision(), 2U);
  EditorPlaytest play;
  EXPECT_FALSE(play.request(document, false));
  EXPECT_EQ(document.policyDenialRevision(), 3U);
  EXPECT_FALSE(play.request(document, false));
  EXPECT_EQ(document.policyDenialRevision(), 4U);
  const EditorLaunchRequest request{*document.path(), document.launchEntry()};
  EXPECT_THROW(static_cast<void>(loadEditorPlayDocument(document, request)),
               std::runtime_error);
  EXPECT_EQ(document.policyDenialRevision(), 5U);
  document.reportResourceError("unrelated resource failure");
  EXPECT_EQ(document.policyDenialRevision(), 5U);
  ASSERT_TRUE(document.save());
  EXPECT_EQ(document.policyDenialRevision(), 5U);
}

TEST_F(RestrictedEditorFiles, NewSavedIdentityStaysPinnedAndIsNotAdoptedByName) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  const auto output = root.path / "output.level.json";
  {
    auto permit = policy->permit(output, EditorFilePolicy::Access::Write);
    ASSERT_TRUE(policy->save(permit, *document.document()));
    std::error_code error;
    std::filesystem::rename(output, root.path / "stolen", error);
    EXPECT_TRUE(error);
  }
  std::filesystem::rename(output, root.path / "retained-owned-output");
  { std::ofstream foreign(output); foreign << "replacement after save"; }
  EXPECT_FALSE(document.saveAs(output));
  EXPECT_EQ(readBytes(output), "replacement after save");
  EXPECT_FALSE(document.open(output));
}

TEST_F(RestrictedEditorFiles, ValidationFinishesBeforeCreatingOrWritingTheOutput) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  auto invalid = *document.document();
  invalid.entries.front().pose.foot_position.y = 1234.0F;
  const auto original = readBytes(root.path / "input.level.json");
  {
    auto permit = policy->permit("output.level.json", EditorFilePolicy::Access::Write);
    EXPECT_FALSE(policy->save(permit, invalid));
    EXPECT_FALSE(std::filesystem::exists(root.path / "output.level.json"));
  }
  {
    auto permit = policy->permit("input.level.json", EditorFilePolicy::Access::Write);
    EXPECT_FALSE(policy->save(permit, invalid));
  }
  EXPECT_EQ(readBytes(root.path / "input.level.json"), original);
}

TEST_F(RestrictedEditorFiles, PartialIoFailureKeepsDirtyStateAndTheOwnedFileIdentity) {
  const auto input = root.path / "input.level.json";
  { std::ofstream padding(input, std::ios::app); padding << std::string(8192, ' '); }
  EditorDocument document(policy);
  ASSERT_TRUE(document.open(input));
  editYaw(document);
  {
    // A read-only mapped section survives closing its original file handle.
    // Windows permits the exclusive writer but refuses truncation while the
    // section exists, giving a real deterministic failure after WriteFile.
    HANDLE original = CreateFileW(input.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(original, INVALID_HANDLE_VALUE);
    HANDLE mapping = CreateFileMappingW(original, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(original);
    ASSERT_NE(mapping, nullptr);
    struct MappedSection {
      HANDLE mapping{};
      void* view{};
      ~MappedSection() {
        if (view) UnmapViewOfFile(view);
        CloseHandle(mapping);
      }
    } section{mapping, MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0)};
    ASSERT_NE(section.view, nullptr);
    EXPECT_FALSE(document.save());
    EXPECT_TRUE(document.dirty());
    EXPECT_EQ(document.path(), input);
    EXPECT_NE(formatLevelDiagnostics(document.diagnostics()).find("partial save"),
              std::string::npos);
  }
  // Retrying is a new explicit Save, using the same originally owned identity.
  ASSERT_TRUE(document.save()) << formatLevelDiagnostics(document.diagnostics());
  EXPECT_FALSE(document.dirty());
  ASSERT_TRUE(document.open(input));
}

TEST_F(RestrictedEditorFiles, PendingOpenRechecksOwnershipBeforeSavingAnything) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  editYaw(document);
  document.requestOpen("other.level.json");
  ASSERT_EQ(document.pendingAction().kind, EditorPendingActionKind::Open);
  const auto unchanged_bytes = readBytes(root.path / "input.level.json");
  std::filesystem::rename(root.path / "other.level.json", root.path / "retained");
  std::filesystem::copy_file(root.path / "retained", root.path / "other.level.json");
  EXPECT_FALSE(document.resolvePending(EditorPendingDecision::Save));
  EXPECT_FALSE(document.resolvePending(EditorPendingDecision::Discard));
  EXPECT_TRUE(document.dirty());
  EXPECT_EQ(document.pendingAction().kind, EditorPendingActionKind::Open);
  EXPECT_EQ(readBytes(root.path / "input.level.json"), unchanged_bytes);
  EXPECT_NE(formatLevelDiagnostics(document.diagnostics()).find("policy_denied"),
            std::string::npos);
  std::filesystem::remove(root.path / "other.level.json");
  std::filesystem::rename(root.path / "retained", root.path / "other.level.json");
  EXPECT_TRUE(document.resolvePending(EditorPendingDecision::Discard));
  EXPECT_FALSE(document.dirty());
}

TEST_F(RestrictedEditorFiles, HardlinksAreRejectedAndReadPermitsPinTheFile) {
  TemporaryRoot outside;
  std::filesystem::create_hard_link(root.path / "input.level.json",
                                    outside.path / "alias.level.json");
  EditorDocument document(policy);
  EXPECT_FALSE(document.open("input.level.json"));
  std::filesystem::remove(outside.path / "alias.level.json");
  ASSERT_TRUE(document.open("input.level.json"));
  auto permit = policy->permit("input.level.json", EditorFilePolicy::Access::Read);
  std::ofstream overwrite(root.path / "input.level.json", std::ios::trunc);
  EXPECT_FALSE(overwrite.is_open());
  std::error_code error;
  std::filesystem::rename(root.path / "input.level.json", root.path / "moved", error);
  EXPECT_TRUE(error);
  EXPECT_TRUE(loadLevelDocument(permit.path()));
}

TEST_F(RestrictedEditorFiles, SymlinksInTheOwnedRootAreRejected) {
  TemporaryRoot outside;
  std::error_code error;
  std::filesystem::create_symlink(outside.path / "absent", root.path / "link", error);
  if (error) GTEST_SKIP() << "Creating symlinks unavailable: " << error.message();
  EditorDocument document(policy);
  EXPECT_FALSE(document.open("input.level.json"));
  EXPECT_NE(formatLevelDiagnostics(document.diagnostics()).find("policy_denied"),
            std::string::npos);
}

TEST_F(RestrictedEditorFiles, JunctionRootsAreRejectedWithoutOpeningTheirFiles) {
  TemporaryRoot outside;
  makeFixture(outside.path / "input.level.json");
  const auto junction = root.path / "junction";
  struct RemoveJunction {
    std::filesystem::path path;
    ~RemoveJunction() { RemoveDirectoryW(path.c_str()); }
  } remove{junction};
  const auto error = createJunction(junction, outside.path);
  if (error) GTEST_SKIP() << "Creating junction unavailable: " << error.message();
  EXPECT_THROW(EditorFilePolicy(junction, {"input.level.json"}, {}),
               std::runtime_error);
  EXPECT_TRUE(loadLevelDocument(outside.path / "input.level.json"));
}

TEST_F(RestrictedEditorFiles, OwnedRootCannotBeRenamedWhileItsPolicyLives) {
  std::error_code error;
  auto moved = root.path;
  moved += "-renamed";
  std::filesystem::rename(root.path, moved, error);
  if (!error) std::filesystem::rename(moved, root.path);
  EXPECT_TRUE(error);
  EXPECT_TRUE(std::filesystem::exists(root.path));
}

TEST_F(RestrictedEditorFiles, RootCannotBecomeAJunctionBetweenPermitAndWrite) {
  TemporaryRoot outside;
  TemporaryRoot probe;
  const auto probe_link = probe.path / "junction";
  const auto support = createJunction(probe_link, outside.path);
  if (support)
    GTEST_SKIP() << "Creating junction unavailable: " << support.message();
  ASSERT_TRUE(RemoveDirectoryW(probe_link.c_str()));

  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  auto permit = policy->permit("output.level.json", EditorFilePolicy::Access::Write);
  // The editable fixture files are deliberately removed after the check. The
  // policy's deny-delete guard still keeps the checked directory nonempty.
  ASSERT_TRUE(std::filesystem::remove(root.path / "input.level.json"));
  ASSERT_TRUE(std::filesystem::remove(root.path / "other.level.json"));
  const auto error = createJunction(root.path, outside.path, false);
  EXPECT_TRUE(error);
  EXPECT_EQ(GetFileAttributesW(root.path.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT,
            0U);
  EXPECT_FALSE(std::filesystem::exists(outside.path / "output.level.json"));
  EXPECT_EQ(permit.path(), root.path / "output.level.json");
  EXPECT_TRUE(policy->save(permit, *document.document()));
  EXPECT_TRUE(std::filesystem::exists(root.path / "output.level.json"));
  EXPECT_FALSE(std::filesystem::exists(outside.path / "output.level.json"));
}

TEST(EditorFilePolicy, OrdinaryAtomicReplacementFailurePreservesExistingBytes) {
  TemporaryRoot root;
  EditorDocument document;
  document.requestNewInterior();
  const auto destination = root.path / "ordinary.level.json";
  ASSERT_TRUE(document.saveAs(destination));
  const auto original = readBytes(destination);
  editYaw(document);
  HANDLE reader = CreateFileW(destination.c_str(), GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(reader, INVALID_HANDLE_VALUE);
  const bool saved = document.save();
  CloseHandle(reader);
  EXPECT_FALSE(saved);
  EXPECT_TRUE(document.dirty());
  EXPECT_EQ(readBytes(destination), original);
  // A failed replacement deletes only its owned sibling temporary file.
  EXPECT_EQ(std::distance(std::filesystem::directory_iterator(root.path),
                          std::filesystem::directory_iterator{}), 1);
  ASSERT_TRUE(document.save());
  EXPECT_FALSE(document.dirty());
  EXPECT_NE(readBytes(destination), original);
}

TEST_F(RestrictedEditorFiles, PlayIsDeniedAtRequestPreflightAndProcessBoundaries) {
  EditorDocument document(policy);
  ASSERT_TRUE(document.open("input.level.json"));
  EditorPlaytest play;
  EXPECT_FALSE(play.request(document, false));
  EXPECT_NE(play.error().find("policy_denied"), std::string::npos);
  const EditorLaunchRequest request{*document.path(), document.launchEntry()};
  EXPECT_THROW(static_cast<void>(loadEditorPlayDocument(document, request)),
               std::runtime_error);
  EditorGameProcess process(false);
  EXPECT_FALSE(process.start(root.path / "does-not-exist.exe", request));
  EXPECT_FALSE(process.active());
  EXPECT_NE(process.status().find("policy_denied"), std::string::npos);
}

TEST(EditorFilePolicyCodecDeathTest, DanglingSiblingTemporaryLinkIsNeverFollowed) {
  TemporaryRoot probe;
  std::error_code error;
  std::filesystem::create_symlink(probe.path / "absent", probe.path / "link", error);
  if (error) GTEST_SKIP() << "Creating symlinks unavailable: " << error.message();
  // A fresh death-test process starts the codec sequence at zero. This checks
  // the underlying writer directly; the restricted policy would reject links
  // even before reaching it. No editor window or game process is involved.
  EXPECT_EXIT(
      {
        const int result = [] {
          TemporaryRoot owned;
          TemporaryRoot outside;
          const auto destination = owned.path / "saved.level.json";
          const auto link = owned.path / "saved.level.json.tmp-0";
          const auto escaped = outside.path / "must-not-be-written";
          std::filesystem::create_symlink(escaped, link);
          EditorDocument document;
          document.requestNewInterior();
          const bool saved = document.saveAs(destination);
          return saved && !std::filesystem::exists(escaped) &&
                         std::filesystem::is_symlink(link) &&
                         loadLevelDocument(destination)
                     ? 0 : 1;
        }();
        std::exit(result);
      }, testing::ExitedWithCode(0), "");
}
#endif
}  // namespace
