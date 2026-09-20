#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

#include "editor/automation/protocol.hpp"

namespace editor_automation {

// Inherited anonymous pipes only. Workers exchange owned JSON values and never
// access ImGui, document state, or an editor object.
class Channel {
 public:
  using Wake = void (*)() noexcept;
  Channel();
  ~Channel();
  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;
  void setWake(Wake wake);
  std::optional<Json> take();
  Json wait(std::chrono::milliseconds timeout);
  void send(const Json& message);
  bool flush(std::chrono::milliseconds timeout);
  [[nodiscard]] bool closed() const noexcept { return closed_.load(); }
  [[nodiscard]] std::string error() const;
  [[nodiscard]] bool cancellationRequested(std::string_view session_id,
                                          std::string_view request_id) const;
  void executionFinished(std::string_view session_id, std::string_view request_id);
 private:
  void read();
  void write();
  void watch();
  void fail(std::string message) noexcept;
  void wake();
  void join(std::thread& thread) noexcept;
  void* input_{};
  void* output_{};
  std::atomic<bool> closed_{};
  mutable std::mutex mutex_;
  std::condition_variable available_;
  std::deque<Json> incoming_;
  std::deque<std::string> outgoing_;
  std::string error_;
  using ExecutionKey = std::pair<std::string, std::string>;
  std::set<ExecutionKey> executions_;
  // Bounded tombstones also cover cancellation received before execution.
  std::set<ExecutionKey> cancellations_;
  bool writing_{};
  std::mutex decoder_mutex_;
  JsonLineDecoder decoder_;
  std::mutex wake_mutex_;
  Wake wake_{};
  std::thread reader_, writer_, watchdog_;
};
}  // namespace editor_automation
