#pragma once

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace editor_automation {
using Json = nlohmann::json;
inline constexpr std::size_t maximum_message_bytes = 1024 * 1024;
inline constexpr std::size_t maximum_json_depth = 32;

class ProtocolError : public std::runtime_error {
 public:
  ProtocolError(std::string code, std::string message)
      : std::runtime_error(message.substr(0, 512)), code_(std::move(code)) {}
  [[nodiscard]] const std::string& code() const noexcept { return code_; }
 private:
  std::string code_;
};

const Json& protocolHello();
void validateHandshake(const Json& value);
void validateRequest(std::string_view tool, const Json& value);
void validateResult(std::string_view tool, const Json& value);
std::string encodeMessage(const Json& value);

// The transport owns this decoder and calls expire even when reads stall.
class JsonLineDecoder {
 public:
  using Clock = std::chrono::steady_clock;
  std::vector<Json> feed(std::string_view bytes, Clock::time_point now = Clock::now());
  void expire(Clock::time_point now = Clock::now());
  void finish();
  [[nodiscard]] bool closed() const noexcept { return closed_; }
  [[nodiscard]] std::size_t bufferedBytes() const noexcept { return buffer_.size(); }
 private:
  [[noreturn]] void fail(std::string code, std::string message);
  std::string buffer_;
  std::optional<Clock::time_point> started_;
  int depth_{};
  bool quoted_{}, escaped_{}, closed_{};
};
}  // namespace editor_automation
