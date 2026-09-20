#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "editor/automation/channel.hpp"
#include "editor/editor_document.hpp"

struct ImGuiTestContext;

namespace editor_automation {
class SemanticUi;
struct SemanticSnapshot;
class EngineSession;

class SessionController {
 public:
  SessionController(Channel& channel, Json configuration);
  ~SessionController();
  SessionController(const SessionController&) = delete;
  SessionController& operator=(const SessionController&) = delete;
  std::shared_ptr<EditorFilePolicy> filePolicy() const { return policy_; }
  const std::filesystem::path& resourceRoot() const { return resource_root_; }
  const std::filesystem::path& initialPath() const { return initial_path_; }
  void attach(EngineSession& engine);
  void detach() noexcept;
  void service(bool ui_available);
  void beginFrame(const EditorDocument& document);
  void publish(const EditorDocument& document, Json preview_fields,
               EditorObjectId preview_actor = 0);
  void dispatch(ImGuiTestContext* context);
  void requestClose();
  bool closing() const noexcept { return closing_ && !batch_; }
  void startupFailed(std::string_view message);

 private:
  using Clock = std::chrono::steady_clock;
  struct Snapshot;
  struct Batch;
  struct Cursor;
  void receive(const Json& message, bool ui_available);
  Json base(const Json& request) const;
  Json error(const Json& request, std::string_view code, std::string_view message,
             std::optional<std::size_t> step = {}, Json target = nullptr) const;
  Json status(const Json& request) const;
  void reply(std::string_view tool, Json response);
  void reject(std::string_view tool, const Json& request, std::string_view code,
              std::string_view message);
  Json observe(const Json& request);
  Json inspect(const Json& request);
  Json evaluate(const Json& condition, bool& passes) const;
  Json action(ImGuiTestContext* context, const Json& step);
  void finishBatch(std::string_view code = {}, std::string_view message = {},
                   bool verified_cleanup = true);
  void releaseInput(ImGuiTestContext* context);
  std::shared_ptr<const Snapshot> snapshot(std::string_view id = {}) const;
  Json stamp(bool stale = false) const;
  void retainResult(const Json& request, const Json& result);

  Channel& channel_;
  Json configuration_;
  Json start_request_;
  std::string session_id_;
  std::filesystem::path resource_root_, initial_path_;
  std::shared_ptr<EditorFilePolicy> policy_;
  std::unique_ptr<SemanticUi> semantic_;
  EngineSession* engine_{};
  std::deque<std::shared_ptr<const Snapshot>> snapshots_;
  std::deque<std::pair<std::string, Json>> pending_reads_;
  std::shared_ptr<Batch> batch_;
  std::map<std::string, std::pair<Json, Json>> results_;
  std::deque<std::string> result_order_;
  std::map<std::string, Cursor> cursors_;
  Json last_request_ = nullptr;
  std::string high_water_{"0"};
  std::uint64_t frame_sequence_{}, cursor_sequence_{};
  Clock::time_point started_{Clock::now()}, activity_{started_};
  bool started_reply_{}, closing_{}, faulted_{}, ui_available_{};
};

// Last application member: also unwinds when its constructor body throws.
class SessionAttachment {
 public:
  SessionAttachment(EngineSession& engine, SessionController* controller);
  ~SessionAttachment();
  SessionAttachment(const SessionAttachment&) = delete;
  SessionAttachment& operator=(const SessionAttachment&) = delete;
 private:
  EngineSession& engine_;
  SessionController& controller_;
};
}  // namespace editor_automation
