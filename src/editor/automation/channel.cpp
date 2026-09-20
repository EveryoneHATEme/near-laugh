#include "editor/automation/channel.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>

namespace editor_automation {
namespace {
constexpr std::size_t queue_limit = 16;
HANDLE duplicatePipe(DWORD standard) {
  const HANDLE source = GetStdHandle(standard);
  if (!source || source == INVALID_HANDLE_VALUE || GetFileType(source) != FILE_TYPE_PIPE)
    throw ProtocolError("invalid_request", "Automation requires inherited input/output pipes");
  HANDLE copy{};
  if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &copy,
                       0, FALSE, DUPLICATE_SAME_ACCESS))
    throw ProtocolError("invalid_request", "Cannot own inherited automation pipe");
  return copy;
}
}  // namespace

Channel::Channel() {
  input_ = duplicatePipe(STD_INPUT_HANDLE);
  try {
    output_ = duplicatePipe(STD_OUTPUT_HANDLE);
    reader_ = std::thread([this] { read(); });
    writer_ = std::thread([this] { write(); });
    watchdog_ = std::thread([this] { watch(); });
  } catch (...) {
    fail("Transport construction failed");
    join(reader_);
    join(writer_);
    join(watchdog_);
    if (output_) CloseHandle(output_);
    CloseHandle(input_);
    throw;
  }
}

Channel::~Channel() {
  setWake(nullptr);
  fail("Transport closed");
  join(reader_);
  join(writer_);
  join(watchdog_);
  CloseHandle(output_);
  CloseHandle(input_);
}

void Channel::join(std::thread& thread) noexcept {
  if (!thread.joinable()) return;
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (WaitForSingleObject(thread.native_handle(), 20) == WAIT_TIMEOUT) {
    CancelSynchronousIo(thread.native_handle());
    if (std::chrono::steady_clock::now() >= end) {
      // No thread may outlive its storage. The host diagnoses owned-child
      // termination and unverified final state; this never fabricates cleanup.
      TerminateProcess(GetCurrentProcess(), 3);
    }
  }
  thread.join();
}

void Channel::setWake(Wake callback) {
  std::lock_guard lock(wake_mutex_);
  wake_ = callback;
}

void Channel::wake() {
  std::lock_guard lock(wake_mutex_);
  if (wake_) wake_();
}

void Channel::fail(std::string message) noexcept {
  {
    std::lock_guard lock(mutex_);
    if (!closed_.exchange(true)) error_ = message.substr(0, 512);
  }
  available_.notify_all();
  wake();
}

std::string Channel::error() const {
  std::lock_guard lock(mutex_);
  return error_;
}

bool Channel::cancellationRequested(std::string_view session_id, std::string_view request_id) const {
  std::lock_guard lock(mutex_);
  return closed_ || cancellations_.contains({std::string(session_id), std::string(request_id)});
}

void Channel::executionFinished(std::string_view session_id, std::string_view request_id) {
  std::lock_guard lock(mutex_);
  const ExecutionKey key{session_id, request_id};
  executions_.erase(key); cancellations_.erase(key);
}

std::optional<Json> Channel::take() {
  std::lock_guard lock(mutex_);
  if (incoming_.empty()) return {};
  Json result = std::move(incoming_.front());
  incoming_.pop_front();
  return result;
}

Json Channel::wait(std::chrono::milliseconds timeout) {
  std::unique_lock lock(mutex_);
  if (!available_.wait_for(lock, timeout, [this] { return closed_ || !incoming_.empty(); }))
    throw ProtocolError("timeout", "Private channel deadline expired");
  if (closed_) throw ProtocolError("disconnected", error_);
  Json result = std::move(incoming_.front());
  incoming_.pop_front();
  return result;
}

void Channel::send(const Json& message) {
  std::string bytes = encodeMessage(message);
  {
    std::lock_guard lock(mutex_);
    if (closed_) throw ProtocolError("disconnected", error_);
    if (outgoing_.size() >= queue_limit)
      throw ProtocolError("limit_exceeded", "Private output queue is full");
    outgoing_.push_back(std::move(bytes));
  }
  available_.notify_all();
}

bool Channel::flush(std::chrono::milliseconds timeout) {
  std::unique_lock lock(mutex_);
  return available_.wait_for(lock, timeout, [this] {
    return closed_ || (outgoing_.empty() && !writing_);
  }) && !closed_;
}

void Channel::read() {
  try {
    std::array<char, 4096> bytes{};
    while (!closed_) {
      DWORD count{};
      if (!ReadFile(input_, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) || count == 0) {
        std::lock_guard decoder_lock(decoder_mutex_);
        decoder_.finish();
        break;
      }
      std::vector<Json> messages;
      {
        std::lock_guard decoder_lock(decoder_mutex_);
        messages = decoder_.feed(std::string_view(bytes.data(), count));
      }
      for (auto& message : messages) {
        bool priority{};
        {
          std::lock_guard lock(mutex_);
          if (incoming_.size() >= queue_limit)
            throw ProtocolError("limit_exceeded", "Private input queue is full");
          if (message.value("kind", "") == "call" && message.value("tool", "") == "ui_execute") {
            const auto& arguments = message.at("arguments");
            validateRequest("ui_execute", arguments);
            const ExecutionKey key{arguments.at("session_id").get<std::string>(), arguments.at("request_id").get<std::string>()};
            if (!executions_.contains(key) && executions_.size() >= queue_limit)
              throw ProtocolError("limit_exceeded", "Private execution queue is full");
            executions_.insert(key);
          }
          if (message.value("kind", "") == "call" && message.value("tool", "") == "ui_session" &&
              message.contains("arguments") && message.at("arguments").is_object()) {
            const auto& arguments = message.at("arguments");
            if (arguments.value("op", "") == "cancel") {
              validateRequest("ui_session", arguments);
              const ExecutionKey key{arguments.at("session_id").get<std::string>(),
                                     arguments.at("active_request_id").get<std::string>()};
              // Priority control can arrive before the matching execution.
              // Retain its identity until executionFinished instead of losing
              // cancellation while the host is still dispatching the batch.
              if (!cancellations_.contains(key) && cancellations_.size() >= queue_limit)
                throw ProtocolError("limit_exceeded", "Private cancellation queue is full");
              cancellations_.insert(key);
              priority = true;
            }
            if (arguments.value("op", "") == "close") priority = true;
          }
          if (priority) incoming_.push_front(std::move(message));
          else incoming_.push_back(std::move(message));
        }
        available_.notify_all();
        wake();
      }
    }
    fail("Controller pipe disconnected");
  } catch (const std::exception& exception) { fail(exception.what()); }
}

void Channel::write() {
  try {
    while (!closed_) {
      std::string bytes;
      {
        std::unique_lock lock(mutex_);
        available_.wait(lock, [this] { return closed_ || !outgoing_.empty(); });
        if (closed_) return;
        bytes = std::move(outgoing_.front());
        outgoing_.pop_front();
        writing_ = true;
      }
      std::size_t offset{};
      while (offset != bytes.size() && !closed_) {
        DWORD written{};
        if (!WriteFile(output_, bytes.data() + offset,
                       static_cast<DWORD>(bytes.size() - offset), &written, nullptr) || written == 0)
          throw ProtocolError("disconnected", "Controller output pipe failed");
        offset += written;
      }
      {
        std::lock_guard lock(mutex_);
        writing_ = false;
      }
      available_.notify_all();
    }
  } catch (const std::exception& exception) { fail(exception.what()); }
}

void Channel::watch() {
  try {
    while (!closed_) {
      {
        std::unique_lock lock(mutex_);
        available_.wait_for(lock, std::chrono::milliseconds(50), [this] { return closed_.load(); });
      }
      if (closed_) return;
      {
        std::lock_guard decoder_lock(decoder_mutex_);
        decoder_.expire();
      }
      // Also wakes minimized waits so deadlines and disconnects remain live.
      wake();
    }
  } catch (const std::exception& exception) { fail(exception.what()); }
}
}  // namespace editor_automation
