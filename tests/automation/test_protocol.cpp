#include <gtest/gtest.h>

#include <limits>

#include "editor/automation/protocol.hpp"

namespace editor_automation {
namespace {
Json request() {
  return {{"protocol_version", 1}, {"session_id", "session"}, {"request_id", "1"},
          {"steps", Json::array({{{"op", "edit"}, {"text", "-"},
                                  {"target", {{"ref", "field"}}}}})}};
}

TEST(AutomationProtocol, UsesThePinnedSharedHandshake) {
  EXPECT_NO_THROW(validateHandshake(protocolHello()));
  auto wrong = protocolHello();
  wrong["build_fingerprint"] = "different";
  try { validateHandshake(wrong); FAIL(); }
  catch (const ProtocolError& error) { EXPECT_EQ(error.code(), "version_mismatch"); }
  wrong = protocolHello();
  wrong["extra"] = true;
  EXPECT_THROW(validateHandshake(wrong), ProtocolError);
}

TEST(AutomationProtocol, PrevalidatesWholeBatchWithoutResolvingTargets) {
  auto value = request();
  EXPECT_NO_THROW(validateRequest("ui_execute", value));
  value["steps"].push_back({{"op", "activate"},
                            {"target", {{"selector", {{"scope", "future-popup"}, {"key", "option"}}}}}});
  EXPECT_NO_THROW(validateRequest("ui_execute", value));
  value["steps"][1]["unexpected"] = true;
  EXPECT_THROW(validateRequest("ui_execute", value), ProtocolError);
  value = request();
  value["steps"][0]["value"] = 4;
  EXPECT_THROW(validateRequest("ui_execute", value), ProtocolError);
  value = request();
  value["timeout_ms"] = true;
  EXPECT_THROW(validateRequest("ui_execute", value), ProtocolError);
}

TEST(AutomationProtocol, RejectsNonfiniteUnsafeAndUnboundedValues) {
  auto value = request();
  value["steps"][0].erase("text");
  value["steps"][0]["value"] = std::numeric_limits<double>::infinity();
  EXPECT_THROW(validateRequest("ui_execute", value), ProtocolError);
  value["steps"][0]["value"] = std::uint64_t{9007199254740992ULL};
  EXPECT_THROW(validateRequest("ui_execute", value), ProtocolError);
  value = request();
  value["steps"][0]["text"] = std::string(16385, 'x');
  EXPECT_THROW(validateRequest("ui_execute", value), ProtocolError);
  value = request();
  value["request_id"] = "01";
  EXPECT_THROW(validateRequest("ui_execute", value), ProtocolError);
}

TEST(AutomationProtocol, FragmentedUtf8AndMultipleLinesRoundTrip) {
  auto value = request();
  value["steps"][0]["text"] = "\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82\nsecond line";
  const auto bytes = encodeMessage(value);
  JsonLineDecoder decoder;
  std::vector<Json> messages;
  for (char ch : bytes) {
    auto next = decoder.feed(std::string_view(&ch, 1));
    messages.insert(messages.end(), next.begin(), next.end());
  }
  ASSERT_EQ(messages.size(), 1U);
  EXPECT_EQ(messages.front(), value);
  EXPECT_EQ(decoder.feed(bytes + bytes).size(), 2U);
  decoder.finish();
}

TEST(AutomationProtocol, RejectsDuplicateKeysInvalidUtf8AndUnterminatedInput) {
  for (const auto& bytes : {std::string("{\"a\":1,\"a\":2}\n"),
                            std::string("{\"a\":\"\xff\"}\n"),
                            std::string("{\"a\":NaN}\n")}) {
    JsonLineDecoder decoder;
    EXPECT_THROW(decoder.feed(bytes), ProtocolError);
    EXPECT_TRUE(decoder.closed());
  }
  JsonLineDecoder decoder;
  EXPECT_TRUE(decoder.feed("{\"partial\":").empty());
  EXPECT_THROW(decoder.finish(), ProtocolError);
}

TEST(AutomationProtocol, FramingLimitsPrecedeRecursiveParsing) {
  JsonLineDecoder nesting;
  EXPECT_THROW(nesting.feed(std::string(33, '[')), ProtocolError);
  EXPECT_EQ(nesting.bufferedBytes(), 0U);
  JsonLineDecoder bytes;
  EXPECT_THROW(bytes.feed(std::string(maximum_message_bytes, ' ')), ProtocolError);
  JsonLineDecoder slow;
  const auto now = JsonLineDecoder::Clock::now();
  EXPECT_TRUE(slow.feed("{", now).empty());
  EXPECT_TRUE(slow.feed(" ", now + std::chrono::seconds(4)).empty());
  EXPECT_THROW(slow.expire(now + std::chrono::seconds(5)), ProtocolError);
  JsonLineDecoder idle;
  EXPECT_NO_THROW(idle.expire(now + std::chrono::hours(2)));
}

TEST(AutomationProtocol, OutputBudgetIncludesFinalDelimiter) {
  Json value{{"x", std::string(maximum_message_bytes, 'x')}};
  EXPECT_THROW(encodeMessage(value), ProtocolError);
}
}  // namespace
}  // namespace editor_automation
