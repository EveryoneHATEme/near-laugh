#include "editor/editor_file_policy.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "core/world/level_document.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace {
[[noreturn]] void denied(const char* reason) {
  throw std::runtime_error(std::string("policy_denied: ") + reason);
}

#if defined(_WIN32)
class FileHandle {
 public:
  explicit FileHandle(HANDLE value = INVALID_HANDLE_VALUE) : value_(value) {}
  ~FileHandle() { reset(); }
  FileHandle(FileHandle&& other) noexcept
      : value_(std::exchange(other.value_, INVALID_HANDLE_VALUE)) {}
  FileHandle& operator=(FileHandle&& other) noexcept {
    if (this != &other) {
      reset();
      value_ = std::exchange(other.value_, INVALID_HANDLE_VALUE);
    }
    return *this;
  }
  FileHandle(const FileHandle&) = delete;
  FileHandle& operator=(const FileHandle&) = delete;
  [[nodiscard]] HANDLE get() const noexcept { return value_; }

 private:
  void reset() noexcept {
    if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_);
  }
  HANDLE value_;
};

struct FileIdentity {
  DWORD volume{};
  DWORD high{}, low{};
  bool operator==(const FileIdentity&) const = default;
};

bool samePath(const std::filesystem::path& first,
              const std::filesystem::path& second) {
  return CompareStringOrdinal(first.c_str(), -1, second.c_str(), -1, TRUE) ==
         CSTR_EQUAL;
}

void checkRaw(const std::filesystem::path& path) {
  const auto& text = path.native();
  if (text.empty() || text.find(L'\0') != std::wstring::npos)
    denied("choose a nonempty level slot path");
  if (text.starts_with(L"\\\\") || text.starts_with(L"//") ||
      text.starts_with(L"\\?") || text.starts_with(L"\\."))
    denied("UNC and device paths are not level slots");
  if (path.has_root_name() != path.has_root_directory())
    denied("drive-relative and root-relative paths are not level slots");
  for (std::size_t index = 0; index < text.size(); ++index)
    if (text[index] == L':' &&
        !(index == 1 && path.has_root_name() && path.is_absolute()))
      denied("alternate data streams are not level slots");
  for (const auto& component : path.relative_path()) {
    const auto part = component.native();
    if (part.empty() || part == L"." || part == L".." ||
        part.back() == L'.' || part.back() == L' ')
      denied("traversal and ambiguous path components are not permitted");
    std::wstring stem = part.substr(0, part.find(L'.'));
    for (auto& letter : stem)
      if (letter >= L'a' && letter <= L'z') letter -= L'a' - L'A';
    if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" ||
        (stem.size() == 4 &&
         (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) &&
         stem[3] >= L'1' && stem[3] <= L'9'))
      denied("reserved device names are not level slots");
  }
}

BY_HANDLE_FILE_INFORMATION info(HANDLE handle) {
  BY_HANDLE_FILE_INFORMATION result{};
  if (!GetFileInformationByHandle(handle, &result))
    denied("cannot verify filesystem ownership");
  if (result.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
    denied("reparse points and links are not permitted");
  return result;
}

FileIdentity identity(const BY_HANDLE_FILE_INFORMATION& value) {
  return {value.dwVolumeSerialNumber, value.nFileIndexHigh, value.nFileIndexLow};
}

FileHandle openChecked(const std::filesystem::path& path, DWORD share) {
  // Attribute-only handles do not participate in all Windows share checks.
  // READ_DATA (LIST_DIRECTORY for a directory) makes the deny-write/delete
  // pin effective while retaining the no-follow reparse-point inspection.
  FileHandle handle(CreateFileW(
      path.c_str(), FILE_READ_ATTRIBUTES | FILE_READ_DATA, share, nullptr,
      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
      nullptr));
  if (handle.get() == INVALID_HANDLE_VALUE)
    denied("cannot pin the owned path for this operation");
  return handle;
}

std::optional<FileIdentity> inspectFile(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    if (GetLastError() == ERROR_FILE_NOT_FOUND) return {};
    denied("cannot inspect the level slot");
  }
  auto handle = openChecked(path, FILE_SHARE_READ | FILE_SHARE_WRITE |
                                     FILE_SHARE_DELETE);
  const auto value = info(handle.get());
  if (value.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    denied("level slots must be regular files");
  if (value.nNumberOfLinks != 1)
    denied("hardlinked files are not owned level slots");
  return identity(value);
}
#endif
}  // namespace

struct EditorFilePolicy::Permit::Impl {
  std::filesystem::path path;
  const EditorFilePolicy* owner{};
  EditorFilePolicy::Access access{};
#if defined(_WIN32)
  FileHandle file_pin;
  std::optional<FileIdentity> expected;
#endif
};

struct EditorFilePolicy::Impl {
  std::filesystem::path root;
#if defined(_WIN32)
  struct Slot {
    std::filesystem::path path;
    std::optional<FileIdentity> owned;
  };
  std::vector<FileHandle> directories;
  std::vector<Slot> slots;
  FileHandle root_contents_pin;

  ~Impl() {
    if (root_contents_pin.get() == INVALID_HANDLE_VALUE) return;
    // Delete the guard through our original handle, never by a reusable name.
    FILE_DISPOSITION_INFO disposition{TRUE};
    SetFileInformationByHandle(root_contents_pin.get(), FileDispositionInfo,
                               &disposition, sizeof(disposition));
  }

  std::filesystem::path resolve(const std::filesystem::path& raw) const {
    checkRaw(raw);
    auto result = (raw.is_absolute() ? raw : root / raw).lexically_normal();
    if (!samePath(result.parent_path(), root))
      denied("path is outside the owned level-file slots");
    return result;
  }

  void checkRoot() const {
    for (const auto& directory : directories) {
      const auto value = info(directory.get());
      if (!(value.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        denied("the owned parent is no longer a directory");
    }
    std::error_code error;
    std::filesystem::directory_iterator iterator(root, error);
    if (error) denied("cannot inspect the owned root");
    std::size_t entries{};
    for (; iterator != std::filesystem::directory_iterator{};
         iterator.increment(error)) {
      if (error || ++entries > 256)
        denied("owned root inspection failed or exceeded its bound");
      // This also finds dangling symlinks that exists() would treat as absent.
      static_cast<void>(inspectFile(iterator->path()));
    }
    if (error) denied("cannot complete owned root inspection");
  }

  Slot& slot(const std::filesystem::path& path) {
    for (auto& candidate : slots)
      if (samePath(candidate.path, path)) return candidate;
    denied("path is not an explicitly owned level-file slot");
  }
#endif
};

EditorFilePolicy::Permit::Permit(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
EditorFilePolicy::Permit::~Permit() = default;
EditorFilePolicy::Permit::Permit(Permit&&) noexcept = default;
EditorFilePolicy::Permit& EditorFilePolicy::Permit::operator=(Permit&&) noexcept =
    default;
const std::filesystem::path& EditorFilePolicy::Permit::path() const noexcept {
  return impl_->path;
}

EditorFilePolicy::EditorFilePolicy(
    std::filesystem::path owned_root,
    std::vector<std::filesystem::path> fixture_files,
    std::vector<std::filesystem::path> output_files)
    : impl_(std::make_unique<Impl>()) {
#if defined(_WIN32)
  checkRaw(owned_root);
  if (!owned_root.is_absolute()) denied("owned root must be absolute");
  if (fixture_files.empty() || fixture_files.size() + output_files.size() > 16)
    denied("a session requires fixtures and at most 16 level-file slots");
  impl_->root = owned_root.lexically_normal();
  auto parent = impl_->root.root_path();
  auto pin = [&](const std::filesystem::path& path) {
    auto handle = openChecked(path, FILE_SHARE_READ | FILE_SHARE_WRITE);
    if (!(info(handle.get()).dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      denied("owned root parents must be directories");
    impl_->directories.push_back(std::move(handle));
  };
  pin(parent);
  for (const auto& component : impl_->root.relative_path()) {
    parent /= component;
    pin(parent);
  }
  // A rename-compatible directory pin allows WRITE sharing. Prevent in-place
  // reparse mutation by keeping its contents nonempty for the policy lifetime.
  // During guard creation, a temporary deny-WRITE directory pin closes the
  // empty-root setup window; it still permits creating ordinary child files.
  auto root_mutation_pin = openChecked(impl_->root, FILE_SHARE_READ);
  if (!(info(root_mutation_pin.get()).dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    denied("owned root is no longer a directory");
  const auto guard_path = impl_->root / ".editor-ui-session.guard";
  impl_->root_contents_pin = FileHandle(CreateFileW(
      guard_path.c_str(), FILE_READ_ATTRIBUTES | FILE_READ_DATA | DELETE,
      FILE_SHARE_READ, nullptr, CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (impl_->root_contents_pin.get() == INVALID_HANDLE_VALUE)
    denied("cannot pin the owned root against reparse mutation");
  impl_->checkRoot();
  auto add = [&](const std::filesystem::path& raw, bool existing) {
    auto path = impl_->resolve(raw);
    if (!path.filename().native().ends_with(L".level.json"))
      denied("slots must use the .level.json suffix");
    for (const auto& previous : impl_->slots)
      if (samePath(previous.path, path)) denied("duplicate level slot");
    auto owned = inspectFile(path);
    if (existing != owned.has_value())
      denied("fixtures must exist and output slots must initially be absent");
    impl_->slots.push_back({std::move(path), owned});
  };
  for (const auto& path : fixture_files) add(path, true);
  for (const auto& path : output_files) add(path, false);
#else
  (void)owned_root;
  (void)fixture_files;
  (void)output_files;
  denied("restricted editor sessions require Windows");
#endif
}

EditorFilePolicy::~EditorFilePolicy() = default;

EditorFilePolicy::Permit EditorFilePolicy::permit(
    const std::filesystem::path& raw_path, Access access) const {
#if defined(_WIN32)
  const auto resolved = impl_->resolve(raw_path);
  impl_->checkRoot();
  const auto& slot = impl_->slot(resolved);
  const auto found = inspectFile(slot.path);
  if (found != slot.owned)
    denied("level slot was replaced or is not owned by this session");
  if (access == Access::Read && !found)
    denied("level slot does not yet contain an owned file");
  auto permit = std::make_unique<Permit::Impl>();
  permit->path = slot.path;
  permit->owner = this;
  permit->access = access;
  permit->expected = found;
  if (found) {
    permit->file_pin = access == Access::Read
        ? openChecked(slot.path, FILE_SHARE_READ)
        : FileHandle(CreateFileW(slot.path.c_str(), GENERIC_WRITE | FILE_READ_ATTRIBUTES,
                                0, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                                nullptr));
    if (permit->file_pin.get() == INVALID_HANDLE_VALUE)
      denied("cannot exclusively pin the owned level slot for writing");
    const auto pinned = info(permit->file_pin.get());
    if (pinned.nNumberOfLinks != 1 || identity(pinned) != *found)
      denied("level slot changed while acquiring its file permit");
  }
  return Permit(std::move(permit));
#else
  (void)raw_path;
  (void)access;
  denied("restricted editor sessions require Windows");
#endif
}

LevelDocumentSaveResult EditorFilePolicy::save(
    Permit& permit, const LevelDocument& document) {
#if defined(_WIN32)
  const auto& path = permit.path();
  auto serialized = serializeLevelDocument(document, path);
  if (!serialized) return {std::move(serialized.diagnostics)};
  bool write_started = false;
  try {
    if (permit.impl_->owner != this || permit.impl_->access != Access::Write)
      denied("Save requires this session's write permit");
    auto& slot = impl_->slot(path);
    if (slot.owned != permit.impl_->expected)
      denied("level slot ownership changed since its write permit was issued");
    if (permit.impl_->file_pin.get() == INVALID_HANDLE_VALUE) {
      // CREATE_NEW makes a competing creation a failure, never a replacement.
      permit.impl_->file_pin = FileHandle(CreateFileW(
          path.c_str(), GENERIC_WRITE | FILE_READ_ATTRIBUTES, 0, nullptr,
          CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
          nullptr));
      if (permit.impl_->file_pin.get() == INVALID_HANDLE_VALUE)
        denied("output slot was occupied or could not be created exclusively");
      const auto created = info(permit.impl_->file_pin.get());
      if (created.nNumberOfLinks != 1)
        denied("new output slot does not have exclusive ownership");
      // Record the exact created object before any write. A failed WriteFile
      // leaves an owned partial output, not an untrusted path to adopt later.
      slot.owned = identity(created);
      permit.impl_->expected = slot.owned;
    }
    const auto handle = permit.impl_->file_pin.get();
    const auto pinned = info(handle);
    if (pinned.nNumberOfLinks != 1 || !slot.owned ||
        identity(pinned) != *slot.owned)
      denied("write permit no longer identifies its owned file");
    auto io_error = [](const char* operation) {
      throw std::system_error(static_cast<int>(GetLastError()),
                              std::system_category(), operation);
    };
    LARGE_INTEGER beginning{};
    if (!SetFilePointerEx(handle, beginning, nullptr, FILE_BEGIN))
      io_error("could not seek the owned temporary level slot");
    write_started = true;
    std::size_t offset{};
    while (offset != serialized.bytes.size()) {
      const auto chunk = static_cast<DWORD>(std::min<std::size_t>(
          serialized.bytes.size() - offset, std::numeric_limits<DWORD>::max()));
      DWORD written{};
      if (!WriteFile(handle, serialized.bytes.data() + offset, chunk, &written, nullptr))
        io_error("could not write the owned temporary level slot");
      if (written == 0)
        throw std::runtime_error("owned temporary level slot made no write progress");
      offset += written;
    }
    if (!SetEndOfFile(handle)) io_error("could not truncate the owned temporary level slot");
    if (!FlushFileBuffers(handle)) io_error("could not flush the owned temporary level slot");
    return {};
  } catch (const std::exception& error) {
    std::string message = error.what();
    if (write_started)
      message += "; this disposable owned slot may contain a partial save";
    return {{{LevelDiagnosticCategory::Filesystem, path, {}, std::move(message)}}};
  }
#else
  (void)permit;
  (void)document;
  denied("restricted editor sessions require Windows");
#endif
}

const std::filesystem::path& EditorFilePolicy::root() const noexcept {
  return impl_->root;
}
