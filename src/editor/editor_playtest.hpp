#ifndef EDITOR_EDITOR_PLAYTEST_HPP
#define EDITOR_EDITOR_PLAYTEST_HPP

#include <memory>

#include "editor/editor_document.hpp"

struct EditorLaunchRequest {
  std::filesystem::path level_path;
  std::string entry_id;
};

enum class EditorPlayState { Idle, ConfirmSave, SaveAs, Ready };

// Preparing a launch snapshots the authored values; launch consumes one
// request.
class EditorPlaytest {
 public:
  bool request(EditorDocument& document, bool child_active);
  bool saveAndPlay(EditorDocument& document);
  bool saveAsAndPlay(EditorDocument& document,
                     const std::filesystem::path& path);
  void cancel();
  [[nodiscard]] std::optional<EditorLaunchRequest> consume();
  [[nodiscard]] EditorPlayState state() const noexcept { return state_; }
  [[nodiscard]] const std::string& error() const noexcept { return error_; }

 private:
  bool unchanged(const EditorDocument& document);
  bool preflight(const EditorDocument& document);
  bool fail(std::string error);
  EditorPlayState state_{};
  std::optional<LevelDocument> prepared_{};
  std::string entry_{};
  std::uint64_t generation_{};
  std::optional<EditorLaunchRequest> launch_{};
  std::string error_{};
};

class EditorGameProcess {
 public:
  EditorGameProcess();
  ~EditorGameProcess();
  EditorGameProcess(const EditorGameProcess&) = delete;
  EditorGameProcess& operator=(const EditorGameProcess&) = delete;
  bool start(const std::filesystem::path& executable,
             const EditorLaunchRequest& request);
  void poll();
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] const std::string& status() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::filesystem::path editorGameExecutable();
#endif
