#include "editor/automation/protocol.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace editor_automation {
namespace {
constexpr std::int64_t maximum_safe_integer = 9007199254740991LL;

const Json& contract() {
  static const Json value = [] {
    std::string bytes;
    for (const std::string_view chunk : {
#include "editor_ui_contract.inc"
    }) bytes.append(chunk);
    return Json::parse(bytes);
  }();
  return value;
}

[[noreturn]] void invalid(std::string_view message) {
  throw ProtocolError("invalid_request", std::string(message));
}
[[noreturn]] void excessive(std::string_view message) {
  throw ProtocolError("limit_exceeded", std::string(message));
}

void checkTree(const Json& value, std::size_t depth = 0) {
  if (depth > maximum_json_depth ||
      (value.is_structured() && depth >= maximum_json_depth))
    excessive("JSON nesting limit exceeded");
  if (value.is_number_unsigned() && value.get<std::uint64_t>() > maximum_safe_integer)
    invalid("Encode large integers as strings");
  if (value.is_number_integer() && !value.is_number_unsigned() &&
      (value.get<std::int64_t>() > maximum_safe_integer ||
       value.get<std::int64_t>() < -maximum_safe_integer))
    invalid("Encode large integers as strings");
  if (value.is_number_float() && !std::isfinite(value.get<double>()))
    invalid("Numbers must be finite");
  if (value.is_structured())
    for (const auto& child : value) checkTree(child, depth + 1);
}

// Deliberately limited to the vocabulary generated from editor_ui_protocol.py.
// No client schema, expression, regex or member traversal is evaluated.
void validate(const Json& value, const Json& schema, std::size_t depth = 0) {
  if (depth > maximum_json_depth) excessive("Schema nesting limit exceeded");
  if (schema.contains("oneOf")) {
    int matches{};
    for (const auto& variant : schema.at("oneOf")) {
      try { validate(value, variant, depth); ++matches; }
      catch (const ProtocolError&) {}
    }
    if (matches != 1) invalid("No unique schema variant");
  }
  const std::string type = schema.value("type", "");
  const bool matching = type.empty() ||
      (type == "object" && value.is_object()) ||
      (type == "array" && value.is_array()) ||
      (type == "string" && value.is_string()) ||
      (type == "integer" && value.is_number_integer()) ||
      (type == "number" && value.is_number()) ||
      (type == "boolean" && value.is_boolean()) ||
      (type == "null" && value.is_null());
  if (!matching) invalid("Incorrect JSON value type");
  if (schema.contains("const")) {
    const auto& expected = schema.at("const");
    if (value != expected ||
        value.is_boolean() != expected.is_boolean() ||
        value.is_number_integer() != expected.is_number_integer())
      invalid("Incorrect constant");
  }
  if (schema.contains("enum") &&
      std::find(schema.at("enum").begin(), schema.at("enum").end(), value) ==
          schema.at("enum").end()) invalid("Unsupported enum value");
  if (type == "object" && schema.contains("properties")) {
    const auto& properties = schema.at("properties");
    for (const auto& key : schema.at("required"))
      if (!value.contains(key.get<std::string>())) invalid("Missing required field");
    if (value.size() < schema.value("minProperties", std::size_t{}))
      invalid("Empty object");
    for (auto it = value.begin(); it != value.end(); ++it) {
      if (!properties.contains(it.key())) invalid("Unknown field");
      validate(it.value(), properties.at(it.key()), depth + 1);
    }
  } else if (type == "array") {
    if (value.size() < schema.at("minItems").get<std::size_t>() ||
        value.size() > schema.at("maxItems").get<std::size_t>())
      excessive("Array size exceeds bounds");
    std::set<std::string> seen;
    for (const auto& child : value) {
      if (schema.value("uniqueItems", false) && !seen.insert(child.dump()).second)
        invalid("Duplicate array value");
      validate(child, schema.at("items"), depth + 1);
    }
  } else if (type == "string") {
    const auto& text = value.get_ref<const std::string&>();
    const auto characters = static_cast<std::size_t>(std::count_if(
        text.begin(), text.end(), [](unsigned char ch) { return (ch & 0xc0) != 0x80; }));
    if (characters < schema.value("minLength", std::size_t{}) ||
        characters > schema.value("maxLength", std::size_t{16384}) ||
        text.size() > schema.value("x-maxUtf8Bytes", std::size_t{16384}))
      excessive("UTF-8 string exceeds bounds");
    if (schema.contains("pattern")) {
      if (schema.at("pattern") != "^[1-9][0-9]*$")
        invalid("Unsupported internal schema pattern");
      if (text.empty() || text.front() < '1' || text.front() > '9' ||
          !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; }))
        invalid("Invalid execution sequence");
    }
  } else if (type == "number" || type == "integer") {
    const double number = value.get<double>();
    if ((schema.contains("minimum") && number < schema.at("minimum").get<double>()) ||
        (schema.contains("maximum") && number > schema.at("maximum").get<double>()) ||
        (schema.contains("exclusiveMinimum") && number <= schema.at("exclusiveMinimum").get<double>()))
      excessive("Number exceeds bounds");
  }
}

Json parseLine(std::string_view line) {
  std::vector<std::set<std::string>> object_keys;
  const auto callback = [&](int, Json::parse_event_t event, Json& parsed) {
    if (event == Json::parse_event_t::object_start) object_keys.emplace_back();
    if (event == Json::parse_event_t::key &&
        !object_keys.back().insert(parsed.get<std::string>()).second)
      invalid("Duplicate JSON object key");
    if (event == Json::parse_event_t::object_end) object_keys.pop_back();
    return true;
  };
  try {
    auto value = Json::parse(line, callback);
    if (!value.is_object()) invalid("Protocol message must be an object");
    checkTree(value);
    return value;
  } catch (const Json::exception&) {
    invalid("Invalid bounded UTF-8 JSON message");
  }
}
}  // namespace

const Json& protocolHello() { return contract().at("hello"); }

void validateHandshake(const Json& value) {
  static_cast<void>(encodeMessage(value));
  validate(value, contract().at("handshake_schema"));
  if (value != protocolHello())
    throw ProtocolError("version_mismatch", "Private protocol/build/dependency/limits mismatch");
}

void validateRequest(std::string_view tool, const Json& value) {
  static_cast<void>(encodeMessage(value));
  const auto& schemas = contract().at("inputs");
  if (!schemas.contains(tool)) invalid("Unknown automation tool");
  validate(value, schemas.at(tool));
  if (value.contains("steps"))
    for (const auto& step : value.at("steps")) {
      if (!step.contains("condition")) continue;
      const auto& condition = step.at("condition");
      if (condition.at("predicate") == "range") {
        const auto& bounds = condition.at("expected");
        if (bounds.contains("min") && bounds.contains("max") && bounds.at("min") > bounds.at("max"))
          invalid("Range minimum exceeds maximum");
      }
    }
}

void validateResult(std::string_view tool, const Json& value) {
  static_cast<void>(encodeMessage(value));
  const auto& schemas = contract().at("outputs");
  if (!schemas.contains(tool)) invalid("Unknown automation tool");
  validate(value, schemas.at(tool));
}

std::string encodeMessage(const Json& value) {
  checkTree(value);
  if (!value.is_object()) invalid("Protocol message must be an object");
  try {
    // Admission/schema bounds cap stored values; serialization has its own
    // channel budget, including the final delimiter.
    std::string bytes = value.dump(-1, ' ', false, Json::error_handler_t::strict);
    if (bytes.size() + 1 > maximum_message_bytes) excessive("Serialized message exceeds byte budget");
    bytes += '\n';
    return bytes;
  } catch (const Json::exception&) {
    invalid("Cannot encode invalid UTF-8 JSON");
  }
}

[[noreturn]] void JsonLineDecoder::fail(std::string code, std::string message) {
  closed_ = true;
  buffer_.clear();
  throw ProtocolError(std::move(code), std::move(message));
}

void JsonLineDecoder::expire(Clock::time_point now) {
  if (closed_) throw ProtocolError("disconnected", "Decoder connection is closed");
  if (started_ && now - *started_ >= std::chrono::seconds(5))
    fail("timeout", "Partial JSON line exceeded its deadline");
}

std::vector<Json> JsonLineDecoder::feed(std::string_view bytes, Clock::time_point now) {
  expire(now);
  if (bytes.size() > maximum_message_bytes) fail("limit_exceeded", "Read fragment exceeds byte budget");
  std::vector<Json> messages;
  for (char ch : bytes) {
    if (!started_) started_ = now;
    if (buffer_.size() + (ch == '\n' ? 1 : 2) > maximum_message_bytes)
      fail("limit_exceeded", "JSON line byte limit exceeded");
    if (ch == '\n') {
      try { messages.push_back(parseLine(buffer_)); }
      catch (const ProtocolError& error) { fail(error.code(), error.what()); }
      buffer_.clear();
      started_.reset();
      depth_ = 0;
      quoted_ = escaped_ = false;
      continue;
    }
    buffer_ += ch;
    if (quoted_) {
      if (escaped_) escaped_ = false;
      else if (ch == '\\') escaped_ = true;
      else if (ch == '"') quoted_ = false;
    } else if (ch == '"') quoted_ = true;
    else if (ch == '{' || ch == '[') {
      if (++depth_ > static_cast<int>(maximum_json_depth))
        fail("limit_exceeded", "JSON nesting limit exceeded before parsing");
    } else if (ch == '}' || ch == ']') {
      if (--depth_ < 0) fail("invalid_request", "Unbalanced JSON delimiters");
    }
  }
  return messages;
}

void JsonLineDecoder::finish() {
  if (closed_) return;
  closed_ = true;
  if (!buffer_.empty()) {
    buffer_.clear();
    throw ProtocolError("disconnected", "Unterminated JSON message at EOF");
  }
}
}  // namespace editor_automation
