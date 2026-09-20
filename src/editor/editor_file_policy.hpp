#pragma once

#include <filesystem>
#include <memory>
#include <vector>

struct LevelDocument;
struct LevelDocumentSaveResult;

// A restricted editor receives this concrete policy from session setup. A null
// policy retains the ordinary editor's filesystem and Play behavior.
class EditorFilePolicy {
 public:
  enum class Access { Read, Write };

  class Permit {
   public:
    ~Permit();
    Permit(Permit&&) noexcept;
    Permit& operator=(Permit&&) noexcept;
    Permit(const Permit&) = delete;
    Permit& operator=(const Permit&) = delete;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

   private:
    friend class EditorFilePolicy;
    struct Impl;
    explicit Permit(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
  };

  // Setup supplies existing fixture copies and absent output filenames, all
  // direct children of its newly owned root. At most 16 slots are accepted.
  EditorFilePolicy(std::filesystem::path owned_root,
                   std::vector<std::filesystem::path> fixture_files,
                   std::vector<std::filesystem::path> output_files);
  ~EditorFilePolicy();
  EditorFilePolicy(const EditorFilePolicy&) = delete;
  EditorFilePolicy& operator=(const EditorFilePolicy&) = delete;

  // Throws a policy_denied diagnostic. Existing files stay pinned through the
  // operation. An absent output is created exclusively only after serialization.
  [[nodiscard]] Permit permit(const std::filesystem::path& raw_path,
                              Access access) const;
  // Automation-only in-place write through the permit's exclusive handle. An
  // I/O failure can leave this disposable slot partially written; it remains
  // owned and returns diagnostics. Ordinary saves retain atomic replacement.
  [[nodiscard]] LevelDocumentSaveResult save(Permit& permit,
                                            const LevelDocument& document);
  [[nodiscard]] const std::filesystem::path& root() const noexcept;
  [[nodiscard]] bool allowsProcessLaunch() const noexcept { return false; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
