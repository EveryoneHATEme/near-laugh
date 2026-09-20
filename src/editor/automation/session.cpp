#include "editor/automation/session.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_te_context.h>

#include <algorithm>
#include <cmath>
#include <set>

#include "editor/automation/application_snapshot.hpp"
#include "editor/automation/engine_session.hpp"
#include "editor/automation/semantic_ui.hpp"
#include "editor/editor_file_policy.hpp"
#include "launcher/executable_path.hpp"

namespace editor_automation {
namespace {
const Json operations = {"activate", "focus", "open", "close", "set_checked", "select",
                         "edit", "commit", "key", "scroll", "drag", "assert", "wait_until"};
bool contains(const Json& list, const Json& value) {
  return std::find(list.begin(), list.end(), value) != list.end();
}
bool later(std::string_view id, std::string_view old) {
  return id.size() > old.size() || (id.size() == old.size() && id > old);
}
std::filesystem::path path(const Json& value) {
  const auto text = value.get<std::string>();
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
Json missingStep(std::size_t index, std::string_view status = "not_run") {
  return {{"index", index}, {"status", status}, {"before", nullptr}, {"after", nullptr}};
}
Json availableAtom(const Json& value) {
  if (value.is_boolean()) return knownValue(value, "boolean");
  if (value.is_number()) return knownValue(value, "float");
  if (value.is_string()) return knownValue(value, "string");
  if (value.is_null()) return knownValue(value, "ref");
  if (value.is_array()) {
    bool strings = true, numbers = true;
    for (const auto& part : value) { strings &= part.is_string(); numbers &= part.is_number(); }
    if (strings) return knownValue(value, "strings");
    if (numbers) return knownValue(value, "numbers");
  }
  if (value.is_object() && value.contains("x")) return knownValue(value, "vector3");
  if (value.is_object() && value.contains("r")) return knownValue(value, "color");
  if (value.is_object() && (value.contains("min") || value.contains("max")))
    return knownValue(value, "range");
  return unavailableValue("Value has no supported observation type");
}
}  // namespace

struct SessionController::Snapshot {
  Json stamp, application, preview;
  SemanticSnapshot ui;
  std::uint64_t policy_denials{};
};
struct SessionController::Batch {
  Json request, before, steps = Json::array();
  Clock::time_point deadline{}, step_deadline{}, cancel_deadline{};
  std::string cancel_code, cancel_message;
  Json failure = nullptr;
  Json assistance = Json::array();
  bool acting{}, effects{};
};
struct SessionController::Cursor {
  std::shared_ptr<const Snapshot> snapshot;
  Json values;
  std::string tool;
  std::size_t offset{}, page_size{};
  bool depth_limited{};
};

SessionController::SessionController(Channel& channel, Json configuration)
    : channel_(channel), configuration_(std::move(configuration)) {
  static const std::set<std::string> keys{"kind", "arguments", "session_id", "root",
      "resource_root", "input_path", "file_slots"};
  if (!configuration_.is_object() || configuration_.size() != keys.size())
    throw ProtocolError("invalid_request", "Invalid private startup configuration");
  for (auto it = configuration_.begin(); it != configuration_.end(); ++it)
    if (!keys.contains(it.key())) throw ProtocolError("invalid_request", "Unknown startup property");
  if (configuration_.at("kind") != "start")
    throw ProtocolError("invalid_request", "Expected private startup configuration");
  start_request_ = configuration_.at("arguments");
  validateRequest("ui_session", start_request_);
  if (start_request_.at("op") != "start") throw ProtocolError("invalid_request", "Expected session start");
  session_id_ = configuration_.at("session_id").get<std::string>();
  if (session_id_.empty() || session_id_.size() > 128)
    throw ProtocolError("invalid_request", "Invalid session identity");
  const auto fixture = start_request_.at("fixture").get<std::string>();
  if (fixture != "apartment-stairs" && fixture != "household-interactions")
    throw ProtocolError("policy_denied", "Fixture is not allowlisted");
  resource_root_ = path(configuration_.at("resource_root"));
  if (resource_root_.lexically_normal() != launcher::executableResourceRoot().lexically_normal())
    throw ProtocolError("policy_denied", "Resources must come from this editor build");
  initial_path_ = path(configuration_.at("input_path"));
  const auto root = path(configuration_.at("root"));
  const auto& slots = configuration_.at("file_slots");
  if (!slots.is_array() || slots.size() != 9 || initial_path_ != root / "input.level.json")
    throw ProtocolError("policy_denied", "Invalid owned file slots");
  std::vector<std::filesystem::path> outputs;
  for (std::size_t index = 0; index != slots.size(); ++index) {
    const auto name = index == 0 ? std::string("input") : "output-" + std::to_string(index);
    const auto& slot = slots[index];
    if (!slot.is_object() || slot.size() != 3 || slot.at("slot") != name ||
        slot.at("writable") != true || path(slot.at("path")) != root / (name + ".level.json"))
      throw ProtocolError("policy_denied", "File slots do not match the fixed manifest");
    if (index != 0) outputs.push_back(path(slot.at("path")));
  }
  policy_ = std::make_shared<EditorFilePolicy>(root,
      std::vector<std::filesystem::path>{initial_path_}, std::move(outputs));
  semantic_ = std::make_unique<SemanticUi>(session_id_);
}
SessionController::~SessionController() { detach(); }
SessionAttachment::SessionAttachment(EngineSession& engine, SessionController* controller)
    : engine_(engine), controller_(controller ? *controller :
        throw std::logic_error("Automation editor requires an owned session")) {
  controller_.attach(engine_);
}
SessionAttachment::~SessionAttachment() { engine_.stop(); controller_.detach(); }
void SessionController::attach(EngineSession& engine) {
  if (engine_) throw std::logic_error("Session already attached");
  engine_ = &engine;
  engine.setDispatcher([](void* owner, ImGuiTestContext* context) {
    static_cast<SessionController*>(owner)->dispatch(context);
  }, this);
}
void SessionController::detach() noexcept {
  if (engine_) engine_->setDispatcher(nullptr, nullptr);
  engine_ = nullptr;
  channel_.setWake(nullptr);
}
Json SessionController::base(const Json& request) const {
  return {{"protocol_version", 1}, {"request_id", request.at("request_id")},
          {"session_id", session_id_}, {"build_fingerprint", protocolHello().at("build_fingerprint")},
          {"ok", true}};
}
Json SessionController::stamp(bool stale) const {
  if (snapshots_.empty()) return nullptr;
  Json result = snapshots_.back()->stamp;
  result["stale"] = stale || !ui_available_;
  return result;
}
Json SessionController::error(const Json&, std::string_view code, std::string_view message,
                              std::optional<std::size_t> step, Json target) const {
  Json result{{"code", code}, {"message", std::string(message.substr(0, 512))},
      {"step_index", step ? Json(*step) : Json(nullptr)}, {"target", target},
      {"last_completed_index", nullptr}, {"expected", nullptr}, {"observed", nullptr},
      {"snapshot", stamp()}, {"effects", "none"}, {"cleanup", "released"},
      {"session_state", faulted_ ? "faulted" : closing_ ? "closing" : batch_ ? "executing" : "ready"}};
  if (!snapshots_.empty()) {
    const auto& ui = snapshots_.back()->ui;
    if (code == "blocked_by_modal" && !ui.modal_scope.is_null()) result["blocking_scope"] = ui.modal_scope;
    if (code == "ambiguous_target" && target.contains("selector")) {
      const auto& selector = target.at("selector");
      result["candidates"] = Json::array();
      for (const auto& item : ui.items) {
        const auto& record = item.record;
        if (record.at("scope") != selector.at("scope") || record.at("key") != selector.at("key")) continue;
        if (selector.contains("owner") && record.at("owner") !=
            (selector.at("owner") == "selection" ? Json(ui.selection_owner) : selector.at("owner"))) continue;
        if (result["candidates"].size() == 256) break;
        result["candidates"].push_back({{"ref", record.at("ref")}, {"scope", record.at("scope")},
            {"key", record.at("key")}, {"owner", record.at("owner")}});
      }
    } else if (!target.is_null()) {
      try {
        const auto& record = semantic_->resolve(target, ui).record;
        result["capabilities"] = record.at("capabilities");
        result["target_state"] = record.at("state");
      }
      catch (const ProtocolError&) {}
    }
  }
  return result;
}
Json SessionController::status(const Json& request) const {
  auto result = base(request);
  result.update({{"state", faulted_ ? "faulted" : closing_ ? "closing" : batch_ ? "executing" :
                                      started_reply_ ? "ready" : "starting"},
      {"capabilities", operations}, {"limits", protocolHello().at("limits")},
      {"file_slots", configuration_.at("file_slots")}, {"last_request", last_request_},
      {"excluded", {"viewport_picking", "placement", "sculpting", "navigation", "gizmos",
                    "docking", "os_dialogs", "game_process"}}});
  return result;
}
void SessionController::reply(std::string_view tool, Json response) {
  validateResult(tool, response);
  channel_.send({{"kind", "result"}, {"tool", tool}, {"result", std::move(response)}});
}
void SessionController::reject(std::string_view tool, const Json& request,
                               std::string_view code, std::string_view message) {
  auto result = base(request);
  result["ok"] = false;
  result["error"] = error(request, code, message);
  if (tool == "ui_execute") {
    result.update({{"before", stamp()}, {"after", stamp()}, {"last_completed_index", nullptr},
        {"failed_index", nullptr}, {"effects", "none"}, {"cleanup", "released"},
        {"session_state", result["error"]["session_state"]}, {"steps", Json::array()}});
    for (std::size_t index = 0; index != request.at("steps").size(); ++index)
      result["steps"].push_back(missingStep(index));
    const auto id = request.at("request_id").get<std::string>();
    if (id == high_water_ && !results_.contains(id) &&
        (!batch_ || batch_->request.at("request_id") != id)) retainResult(request, result);
    if (!batch_ || request.value("session_id", "") != session_id_ || batch_->request.at("request_id") != id)
      channel_.executionFinished(request.value("session_id", ""), id);
  }
  reply(tool, std::move(result));
}
void SessionController::startupFailed(std::string_view message) {
  faulted_ = closing_ = true;
  if (batch_) finishBatch("engine_error", message, false);
  else if (!started_reply_) reject("ui_session", start_request_, "environment_unavailable", message);
}
void SessionController::requestClose() { closing_ = true; }

void SessionController::service(bool ui_available) {
  ui_available_ = ui_available;
  const auto now = Clock::now();
  while (const auto message = channel_.take()) receive(*message, ui_available);
  if (channel_.closed() || now - started_ >= std::chrono::minutes(60) ||
      (!batch_ && now - activity_ >= std::chrono::minutes(5))) closing_ = true;
  if (batch_) {
    std::string code;
    if (channel_.closed()) code = "disconnected";
    else if (closing_ || channel_.cancellationRequested(session_id_, batch_->request.at("request_id").get<std::string>())) code = "cancelled";
    else if (now >= batch_->deadline || (batch_->acting && now >= batch_->step_deadline)) code = "timeout";
    else if (!ui_available) code = "ui_unavailable";
    if (!code.empty() && batch_->cancel_code.empty()) {
      batch_->cancel_code = code;
      batch_->cancel_message = "Request stopped by session control or deadline";
      batch_->cancel_deadline = now + std::chrono::seconds(2);
    }
    if (!batch_->cancel_code.empty() && now >= batch_->cancel_deadline) {
      faulted_ = closing_ = true;
      finishBatch(batch_->cancel_code, "No completed release frame before cleanup deadline", false);
    }
  }
  if (engine_ && engine_->snapshot().state == EngineSessionState::Faulted) {
    faulted_ = closing_ = true;
    if (batch_) finishBatch("engine_error", "Persistent Test Engine dispatcher failed", false);
  }
  if (!ui_available) {
    while (!pending_reads_.empty()) {
      auto [tool, request] = std::move(pending_reads_.front());
      pending_reads_.pop_front();
      reject(tool, request, "ui_unavailable", "No fresh UI frame is available");
    }
  }
}

void SessionController::receive(const Json& message, bool ui_available) {
  if (!message.is_object() || message.size() != 3 || message.value("kind", "") != "call")
    throw ProtocolError("invalid_request", "Invalid private tool envelope");
  const std::string tool = message.at("tool").get<std::string>();
  const auto& request = message.at("arguments");
  validateRequest(tool, request);
  activity_ = Clock::now();
  try {
    if (request.contains("session_id") && request.at("session_id") != session_id_)
      throw ProtocolError("session_closed", "Request belongs to a different session");
    if (tool == "ui_session") {
      const auto op = request.at("op").get<std::string>();
      if (op == "start") throw ProtocolError("busy", "This editor already has a session owner");
      if (op == "cancel" && batch_ && batch_->cancel_code.empty() &&
          request.at("active_request_id") == batch_->request.at("request_id")) {
        batch_->cancel_code = "cancelled";
        batch_->cancel_message = "Controller cancelled the request";
        batch_->cancel_deadline = Clock::now() + std::chrono::seconds(2);
      }
      if (op == "close") requestClose();
      reply(tool, status(request));
      return;
    }
    if (closing_ || faulted_) throw ProtocolError("session_faulted", "Session no longer accepts work");
    if (tool == "ui_execute") {
      const auto id = request.at("request_id").get<std::string>();
      if (const auto found = results_.find(id); found != results_.end()) {
        if (found->second.first != request) throw ProtocolError("request_id_conflict", "Request ID has different content");
        reply(tool, found->second.second);
        channel_.executionFinished(session_id_, id);
        return;
      }
      if (batch_ && batch_->request.at("request_id") == id) {
        if (batch_->request != request) throw ProtocolError("request_id_conflict", "Active request ID has different content");
        auto result = base(request);
        result.update({{"status", "running"}, {"session_state", "executing"}});
        reply(tool, std::move(result));
        return;
      }
      if (!later(id, high_water_)) throw ProtocolError("result_expired", "Request ID cannot be replayed");
      if (batch_) throw ProtocolError("busy", "Another batch is executing");
      if (!ui_available || snapshots_.empty()) throw ProtocolError("ui_unavailable", "No completed UI frame is available");
      high_water_ = id;
      if (request.contains("if_state")) {
        for (auto guard = request["if_state"].begin(); guard != request["if_state"].end(); ++guard)
          if (snapshots_.back()->stamp.at(guard.key()) != guard.value())
            throw ProtocolError("state_conflict", "Initial document revision guard failed");
      }
      batch_ = std::make_shared<Batch>();
      batch_->request = request;
      batch_->before = stamp();
      batch_->deadline = Clock::now() + std::chrono::milliseconds(request.value("timeout_ms", 20000));
      last_request_ = {{"request_id", id}, {"status", "running"}, {"error", nullptr}};
    } else if (request.contains("cursor") || request.contains("snapshot_id")) {
      reply(tool, tool == "ui_observe" ? observe(request) : inspect(request));
    } else {
      if (!ui_available) throw ProtocolError("ui_unavailable", "No fresh UI frame is available");
      if (pending_reads_.size() >= 16) throw ProtocolError("limit_exceeded", "Read queue is full");
      pending_reads_.emplace_back(tool, request);
    }
  } catch (const ProtocolError& exception) { reject(tool, request, exception.code(), exception.what()); }
}

void SessionController::beginFrame(const EditorDocument& document) { semantic_->beginFrame(document); }
void SessionController::publish(const EditorDocument& document, Json preview_fields, EditorObjectId preview_actor) {
  auto next = std::make_shared<Snapshot>();
  next->ui = semantic_->finishFrame(document);
  next->policy_denials = document.policyDenialRevision();
  const auto actor = next->ui.object_refs.find(preview_actor);
  preview_fields["character.actor"] = knownValue(actor == next->ui.object_refs.end() ? Json(nullptr) : Json(actor->second), "ref");
  next->preview = std::move(preview_fields);
  const auto previous = snapshots_.empty() ? nullptr : snapshots_.back();
  std::uint64_t preview_revision = previous ? std::stoull(previous->stamp.at("preview_revision").get<std::string>()) : 0;
  if (!previous || previous->preview != next->preview) ++preview_revision;
  ++frame_sequence_;
  next->stamp = {{"snapshot_id", session_id_ + ":snapshot:" + std::to_string(frame_sequence_)},
      {"frame", std::to_string(frame_sequence_)}, {"document_generation", std::to_string(document.generation())},
      {"document_revision", std::to_string(document.revision())},
      {"selection_revision", std::to_string(document.selectionRevision())},
      {"preview_revision", std::to_string(preview_revision)}, {"stale", false}};
  next->application = captureApplication(document, next->ui.object_refs, next->preview);
  Json slot = nullptr;
  for (const auto& entry : configuration_.at("file_slots"))
    if (path(entry.at("path")) == document.path()) slot = entry.at("slot");
  next->application.push_back({{"projection", "document"}, {"field", "slot"},
      {"object_ref", nullptr}, {"value", knownValue(slot, "optional_string")}, {"source", "document"}});
  snapshots_.push_back(next);
  while (snapshots_.size() > 8) {
    snapshots_.pop_front();
  }
  if (!started_reply_) { started_reply_ = true; reply("ui_session", status(start_request_)); }
  while (!pending_reads_.empty()) {
    auto [tool, request] = std::move(pending_reads_.front()); pending_reads_.pop_front();
    try { reply(tool, tool == "ui_observe" ? observe(request) : inspect(request)); }
    catch (const ProtocolError& exception) { reject(tool, request, exception.code(), exception.what()); }
  }
}
std::shared_ptr<const SessionController::Snapshot> SessionController::snapshot(std::string_view id) const {
  if (snapshots_.empty()) throw ProtocolError("ui_unavailable", "No completed frame exists");
  if (id.empty()) return snapshots_.back();
  for (const auto& value : snapshots_)
    if (value->stamp.at("snapshot_id").get<std::string>() == id) return value;
  for (const auto& [key, cursor] : cursors_)
    if (cursor.snapshot->stamp.at("snapshot_id").get<std::string>() == id) return cursor.snapshot;
  throw ProtocolError("snapshot_expired", "Snapshot is no longer retained");
}

Json SessionController::observe(const Json& request) {
  Cursor page;
  if (request.contains("cursor")) {
    const auto found = cursors_.find(request.at("cursor").get<std::string>());
    if (found == cursors_.end() || found->second.tool != "ui_observe")
      throw ProtocolError("snapshot_expired", "Observation cursor expired");
    page = found->second;
  } else {
    page.snapshot = snapshot(); page.tool = "ui_observe";
    page.page_size = request.value("page_size", std::size_t{128});
    page.values = Json::array();
    const auto scope = request.at("scope").get<std::string>();
    const auto depth = request.value("depth", 1U);
    const auto& parents = page.snapshot->ui.scope_parents;
    if (scope != "root" && !parents.contains(scope))
      throw ProtocolError("stale_ref", "Scope is not present in this frame");
    for (const auto& item : page.snapshot->ui.items) {
      auto parent = item.record.at("scope").get<std::string>();
      unsigned level = 0;
      std::set<std::string> seen;
      while (parent != scope && parent != "root" && parents.contains(parent) && seen.insert(parent).second) {
        parent = parents.at(parent); ++level;
      }
      if (parent != scope) continue;
      if (level > depth) { page.depth_limited = true; continue; }
      bool matches = true;
      if (request.contains("filter"))
        for (auto filter = request["filter"].begin(); filter != request["filter"].end(); ++filter) {
          Json value = filter.value();
          if (filter.key() == "owner" && value == "selection") value = page.snapshot->ui.selection_owner;
          matches &= item.record.at(filter.key()) == value;
        }
      if (matches) page.values.push_back(item.record);
    }
  }
  auto result = base(request);
  result.update({{"snapshot", page.snapshot->stamp}, {"active_scope", page.snapshot->ui.active_scope},
      {"modal_scope", page.snapshot->ui.modal_scope}, {"items", Json::array()},
      {"coverage", page.snapshot->ui.coverage}, {"next_cursor", nullptr}});
  result["snapshot"]["stale"] = !ui_available_;
  if (page.depth_limited) result["coverage"]["limitations"].push_back("depth_limit");
  const auto end = std::min(page.values.size(), page.offset + page.page_size);
  std::size_t bytes = result.dump().size();
  while (page.offset < end) {
    const auto size = page.values[page.offset].dump().size();
    // The MCP response duplicates JSON as text, where escaping can double it.
    if (bytes + size > 200 * 1024 && !result["items"].empty()) break;
    if (bytes + size > 200 * 1024) throw ProtocolError("limit_exceeded", "Item exceeds page byte budget");
    bytes += size; result["items"].push_back(page.values[page.offset++]);
  }
  if (page.offset < page.values.size()) {
    const auto cursor = session_id_ + ":cursor:" + std::to_string(++cursor_sequence_);
    if (cursors_.size() >= 16) cursors_.erase(cursors_.begin());
    cursors_[cursor] = std::move(page); result["next_cursor"] = cursor;
    if (!contains(result["coverage"]["limitations"], "pagination"))
      result["coverage"]["limitations"].push_back("pagination");
  }
  return result;
}

Json SessionController::inspect(const Json& request) {
  Cursor page;
  if (request.contains("cursor")) {
    const auto found = cursors_.find(request.at("cursor").get<std::string>());
    if (found == cursors_.end() || found->second.tool != "app_inspect")
      throw ProtocolError("snapshot_expired", "Inspection cursor expired");
    page = found->second;
  } else {
    page.snapshot = snapshot(request.value("snapshot_id", "")); page.tool = "app_inspect";
    page.page_size = request.value("page_size", std::size_t{128}); page.values = Json::array();
    const auto projection = request.at("projection").get<std::string>();
    const auto concrete_projection = projection == "objects" ? "object" : projection;
    const auto object = request.value("object_ref", "");
    if (projection == "object") {
      bool found{};
      for (const auto& [id, ref] : page.snapshot->ui.object_refs) found |= ref == object;
      if (!found) throw ProtocolError("stale_ref", "Object does not exist in the requested snapshot");
    }
    std::set<std::string> fields;
    for (auto value : page.snapshot->application) {
      if (value.at("projection") != concrete_projection ||
          (!object.empty() && value.at("object_ref") != object) ||
          (request.contains("fields") && !contains(request.at("fields"), value.at("field")))) continue;
      fields.insert(value.at("field").get<std::string>());
      value["projection"] = projection; page.values.push_back(std::move(value));
    }
    if (request.contains("fields") && projection != "objects" && projection != "diagnostics")
      for (const auto& field : request.at("fields")) {
        if (fields.contains(field.get<std::string>())) continue;
        page.values.push_back({{"projection", projection}, {"field", field},
            {"object_ref", object.empty() ? Json(nullptr) : Json(object)},
            {"value", unavailableValue("Field does not apply to this record", "not_applicable")},
            {"source", projection == "object" ? "document" : projection}});
      }
  }
  auto result = base(request);
  result.update({{"snapshot", page.snapshot->stamp}, {"values", Json::array()},
                 {"next_cursor", nullptr}, {"truncated", false}});
  result["snapshot"]["stale"] = !ui_available_;
  const auto end = std::min(page.values.size(), page.offset + page.page_size);
  std::size_t bytes = result.dump().size();
  while (page.offset < end) {
    const auto size = page.values[page.offset].dump().size();
    if (bytes + size > 200 * 1024 && !result["values"].empty()) break;
    if (bytes + size > 200 * 1024) throw ProtocolError("limit_exceeded", "Value exceeds page byte budget");
    bytes += size; result["values"].push_back(page.values[page.offset++]);
  }
  if (page.offset < page.values.size()) {
    const auto cursor = session_id_ + ":cursor:" + std::to_string(++cursor_sequence_);
    if (cursors_.size() >= 16) cursors_.erase(cursors_.begin());
    cursors_[cursor] = std::move(page); result["next_cursor"] = cursor;
  }
  return result;
}

Json SessionController::evaluate(const Json& condition, bool& passes) const {
  const auto current = snapshot();
  const auto predicate = condition.at("predicate").get<std::string>();
  const auto field = condition.at("field").get<std::string>();
  Json observed = unavailableValue("Field is not available in this frame");
  bool exists = false;
  if (condition.at("source") == "ui") {
    try {
      const auto& item = semantic_->resolve(condition.at("target"), current->ui).record;
      exists = true;
      if (field == "value.draft") {
        observed = item.at("value");
        if (observed.contains("draft")) { observed["value"] = observed["draft"]; observed.erase("draft"); }
      } else if (field == "value.availability") observed = availableAtom(item.at("value").at("availability"));
      else if (field == "count") observed = knownValue(1, "integer");
      else if (field.starts_with("state.")) {
        const auto& state = item.at("state").at(field.substr(6));
        observed = state.is_boolean() ? knownValue(state, "boolean") : state;
      } else if (field.starts_with("input.")) {
        const auto& input = item.at("input");
        if (field == "input.availability") observed = availableAtom(input.at("availability"));
        else if (input.at("availability") == "known") observed = availableAtom(input.at(field.substr(6)));
        else if (field == "input.text" && input.at("availability") == "truncated")
          observed = {{"availability", "truncated"}, {"type", "string"}, {"value", input.at("text")},
                      {"reason", input.at("reason")}};
        else observed = unavailableValue("Active input is unavailable");
      } else if (field == "input_validation.status") {
        observed = item.contains("input_validation") ? availableAtom(item.at("input_validation").at("status"))
                                                     : unavailableValue("This widget has no input syntax observation");
      } else if (field == "label" && item.value("label_availability", "known") == "truncated") {
        observed = {{"availability", "truncated"}, {"type", "string"}, {"value", item.at("label")},
                    {"reason", "Full label exceeds the observation byte limit"}};
      } else if (field == "commit.policy") observed = availableAtom(item.at("commit").at("policy"));
      else observed = availableAtom(item.at(field));
    } catch (const ProtocolError& exception) {
      if (predicate != "exists" || exception.code() != "not_found") throw;
      // A selector missing from submitted items is inconclusive if coverage is incomplete.
      const auto& limits = current->ui.coverage.at("limitations");
      for (const auto& limit : limits)
        if (limit != "submitted_only") throw ProtocolError("value_unavailable", "Coverage cannot prove target absence");
    }
  } else {
    auto projection = condition.at("projection").get<std::string>();
    if (projection == "objects") projection = "object";
    std::vector<Json> matches;
    for (const auto& value : current->application) {
      if (value.at("projection") != projection || value.at("field") != field ||
          (condition.contains("object_ref") && value.at("object_ref") != condition.at("object_ref"))) continue;
      matches.push_back(value.at("value"));
    }
    exists = !matches.empty();
    if (predicate == "count") observed = knownValue(matches.size(), "integer");
    else if (matches.size() == 1) observed = matches.front();
    else if (matches.size() > 1) throw ProtocolError("ambiguous_target", "Projection condition matches multiple values");
  }
  if (predicate == "exists") {
    // Existence predicates do not turn unavailable values into verified absence.
    if (exists && observed.value("availability", "unknown") != "known")
      return observed;
    passes = condition.at("expected") == exists;
    return knownValue(exists, "boolean");
  }
  if (observed.value("availability", "unknown") != "known")
    return observed;
  const auto& actual = observed.at("value");
  const auto& expected = condition.at("expected");
  if (predicate == "equals" || predicate == "not_equals") {
    const bool same_type = actual.type() == expected.type() || (actual.is_number() && expected.is_number());
    if (!same_type) throw ProtocolError("invalid_request", "Equality operands have different types");
    passes = predicate == "equals" ? actual == expected : actual != expected;
  } else if (predicate == "range") {
    if (!actual.is_number()) throw ProtocolError("invalid_request", "Range requires a number");
    const double number = actual.get<double>();
    passes = (!expected.contains("min") || number >= expected.at("min").get<double>()) &&
             (!expected.contains("max") || number <= expected.at("max").get<double>());
  } else if (predicate == "approx") {
    if (!actual.is_number()) throw ProtocolError("invalid_request", "Approximation requires a number");
    const double target = expected.get<double>();
    const auto& tolerance = condition.at("tolerance");
    passes = std::abs(actual.get<double>() - target) <=
        std::max(tolerance.value("absolute", 0.), tolerance.value("relative", 0.) * std::abs(target));
  } else if (predicate == "contains") {
    if (!actual.is_string()) throw ProtocolError("invalid_request", "Literal contains requires a string");
    passes = actual.get_ref<const std::string&>().find(expected.get<std::string>()) != std::string::npos;
  } else if (predicate == "count") {
    if (actual.is_array()) passes = actual.size() == expected.get<std::size_t>();
    else if (actual.is_number_integer()) passes = actual == expected;
    else throw ProtocolError("invalid_request", "Count requires a collection");
  }
  return observed;
}

Json SessionController::action(ImGuiTestContext* context, const Json& step) {
  const auto active = batch_;
  const auto check_control = [&] {
    if (context->IsError()) throw ProtocolError("engine_error", "Engine stopped during interaction");
    if (batch_ != active || !active) throw ProtocolError("cancelled", "Request is no longer active");
    if (!active->cancel_code.empty()) throw ProtocolError(active->cancel_code, active->cancel_message);
    if (channel_.cancellationRequested(session_id_, active->request.at("request_id").get<std::string>()))
      throw ProtocolError("cancelled", "Controller cancelled the request");
    if (Clock::now() >= active->step_deadline) throw ProtocolError("timeout", "Operation deadline expired");
  };
  check_control();
  const auto current = snapshot();
  const auto item = semantic_->resolve(step.at("target"), current->ui);
  const auto op = step.at("op").get<std::string>();
  const auto& record = item.record;
  if (!contains(record.at("capabilities"), op))
    throw ProtocolError("unsupported", "Target does not advertise this operation");
  const auto& enabled = record.at("state").at("enabled");
  if (!enabled.is_boolean()) throw ProtocolError("value_unavailable", "Enabled state is not known");
  if (!enabled.get<bool>()) throw ProtocolError("disabled", "Target is disabled");
  if (!current->ui.modal_scope.is_null()) {
    auto scope = record.at("scope").get<std::string>();
    const auto modal = current->ui.modal_scope.get<std::string>();
    std::set<std::string> seen;
    while (scope != modal && current->ui.scope_parents.contains(scope) && seen.insert(scope).second)
      scope = current->ui.scope_parents.at(scope);
    if (scope != modal && record.at("ref") != modal)
      throw ProtocolError("blocked_by_modal", "A modal scope blocks this target");
  }
  if (item.window.empty()) throw ProtocolError("unsupported", "Target has no interactive window");
  // Window names are absolute ImGui identities, not paths relative to RefID.
  // Numeric refs also preserve literal slashes in generated child names.
  const auto window_reference = ImGuiTestRef(ImHashStr(item.window.c_str()));
  struct RestoreFlags {
    ImGuiTestContext* context;
    ImGuiTestOpFlags previous;
    ~RestoreFlags() { context->OpFlags = previous; }
  } restore_flags{context, context->OpFlags};
  context->OpFlags |= ImGuiTestOpFlags_NoAutoUncollapse;
  context->SetRef(window_reference);
  const auto reference = ImGuiTestRef(item.id);
  const auto input = [&] { if (batch_) batch_->effects = true; };
  // The pinned engine's MouseMove/ItemClick helpers may resize/teleport windows,
  // uncollapse ancestors and dismiss popups. Aim from engine facts, then inject
  // its primitive input instead, with each permitted convenience explicit.
  const auto policy = active->request.value("policy", Json::object());
  auto* target_window = ImGui::FindWindowByName(item.window.c_str());
  if (!target_window || target_window->Collapsed || target_window->Hidden)
    throw ProtocolError("ui_unavailable", "Target window is hidden or collapsed; open it explicitly");
  const auto& popup_stack = ImGui::GetCurrentContext()->OpenPopupStack;
  if (!popup_stack.empty()) {
    auto* popup = popup_stack.back().Window;
    if (popup && target_window != popup && !ImGui::IsWindowChildOf(target_window, popup, true, true)) {
      bool owns_popup = false;
      for (const auto& candidate : current->ui.items)
        if (candidate.window == popup->Name) {
          const auto scope = candidate.record.at("scope").get<std::string>();
          const auto origin = current->ui.scope_items.find(scope);
          owns_popup |= origin != current->ui.scope_items.end() && Json(origin->second) == record.at("ref");
        }
      if (!(op == "close" && owns_popup) && !(op == "open" && record.at("kind") == "menu"))
        throw ProtocolError("state_conflict", "An open popup must be closed explicitly before targeting another scope");
    }
  }
  const auto revalidate = [&] {
    check_control();
    const auto& now = semantic_->resolve(step.at("target"), snapshot()->ui);
    if (now.id != item.id || now.window != item.window)
      throw ProtocolError("state_conflict", "Target was recreated during interaction; observe again");
  };
  const auto assist = [&](std::string_view action, const auto& perform) {
    const auto index = active->assistance.size();
    active->assistance.push_back({{"action", action}, {"target", step.at("target")},
                                  {"before", stamp()}, {"after", stamp()}});
    input();
    try { perform(); }
    catch (...) { active->assistance[index]["after"] = stamp(); throw; }
    active->assistance[index]["after"] = stamp();
    revalidate();
  };
  const auto prepare = [&]() {
    auto* window = ImGui::FindWindowByName(item.window.c_str());
    if (!window || window->Collapsed || window->Hidden)
      throw ProtocolError("ui_unavailable", "Target window is hidden or collapsed; open it explicitly");
    const auto& gui = *ImGui::GetCurrentContext();
    if (!gui.OpenPopupStack.empty()) {
      auto* popup = gui.OpenPopupStack.back().Window;
      if (popup && window != popup && !ImGui::IsWindowChildOf(window, popup, true, true) &&
          !((op == "open" || op == "close") && record.at("kind") == "menu"))
        throw ProtocolError("state_conflict", "An open popup must be closed explicitly before targeting another scope");
    }
    auto facts = context->ItemInfo(reference, ImGuiTestOpFlags_NoError);
    check_control();
    if (!facts.ID || !facts.Window)
      throw ProtocolError("ui_unavailable", "Test Engine has no current target geometry");
    if (facts.ItemFlags & ImGuiItemFlags_Disabled)
      throw ProtocolError("disabled", "Current engine item is disabled");
    const auto clipped = [&] {
      if (op == "focus")
        return !facts.RectClipped.Contains(facts.RectFull);
      return facts.RectClipped.GetWidth() + 1.F < facts.RectFull.GetWidth() * .7F ||
             facts.RectClipped.GetHeight() + 1.F < facts.RectFull.GetHeight() * .9F;
    };
    if (clipped()) {
      if (!policy.value("auto_scroll", false))
        throw ProtocolError("ui_unavailable", "Target is clipped; request scroll or explicitly permit auto_scroll");
      assist("scroll", [&] {
        context->ScrollToItem(reference, ImGuiAxis_X, ImGuiTestOpFlags_NoFocusWindow);
        check_control();
        context->ScrollToItem(reference, ImGuiAxis_Y, ImGuiTestOpFlags_NoFocusWindow);
        context->Yield();
      });
      facts = context->ItemInfo(reference, ImGuiTestOpFlags_NoError);
      if (!facts.ID || clipped())
        throw ProtocolError("ui_unavailable", "Target remains clipped after permitted scrolling");
    }
    ImVec2 aim = facts.RectClipped.GetCenter();
    if (aim.x < 0 || aim.y < 0 || aim.x >= ImGui::GetIO().DisplaySize.x || aim.y >= ImGui::GetIO().DisplaySize.y)
      throw ProtocolError("ui_unavailable", "Target is outside the display; automatic window movement is unsupported");
    const auto receives_pointer = [&] {
      auto* hovered = context->FindHoveredWindowAtPos(aim);
      // InputTextMultiline owns a child window. It is part of the observed
      // widget; the exact HoveredId check below still rejects other controls.
      return hovered == facts.Window || (hovered && (hovered->Flags & ImGuiWindowFlags_ChildWindow) &&
          ImGui::IsWindowChildOf(hovered, facts.Window, false, true));
    };
    if (!receives_pointer()) {
      if (!policy.value("auto_focus", false))
        throw ProtocolError("ui_unavailable", "Another window covers the target; focus explicitly or permit auto_focus");
      assist("focus", [&] { context->WindowFocus(window_reference); context->Yield(); });
      facts = context->ItemInfo(reference, ImGuiTestOpFlags_NoError);
      if (!facts.ID) throw ProtocolError("ui_unavailable", "Target disappeared while focusing");
      aim = facts.RectClipped.GetCenter();
      if (!receives_pointer())
        throw ProtocolError("ui_unavailable", "Target remains covered after permitted focus");
    }
    revalidate();
    if (facts.TimestampMain < ImGui::GetFrameCount() - 1)
      throw ProtocolError("ui_unavailable", "Engine geometry became stale before input");
    input(); context->MouseSetViewport(facts.Window);
    context->MouseTeleportToPos(aim); context->Yield(); check_control();
    // No attempts to move an obstruction or retry a different control.
    if (ImGui::GetCurrentContext()->HoveredId != item.id)
      throw ProtocolError("ui_unavailable", "The observed widget does not receive pointer input at its engine target");
    return facts;
  };
  const auto click = [&] { prepare(); context->MouseClick(); check_control(); };
  if (op == "commit") {
    if (record.at("state").at("active") != true)
      throw ProtocolError("state_conflict", "Commit target is not the active input");
    if (!contains(record.at("commit").at("methods"), step.at("method")))
      throw ProtocolError("unsupported", "Target does not support that commit method");
    input(); context->KeyPress(step.at("method") == "enter" ? ImGuiKey_Enter : ImGuiKey_Tab);
  } else if (op == "activate" || op == "select") { click(); }
  else if (op == "focus") {
    if (record.at("kind") == "window" || record.at("kind") == "child") {
      input(); context->WindowFocus(window_reference);
    }
    else { prepare(); context->NavMoveTo(reference); }
  } else if (op == "open") {
    if (record.at("kind") == "combo") {
      // BeginCombo does not emit the engine Opened flag in the pinned ImGui.
      // Its UI-local open fact determines whether a real click is needed.
      const auto& open = record.at("state").at("open");
      if (!open.is_boolean()) throw ProtocolError("value_unavailable", "Combo open state is unavailable");
      if (!open.get<bool>()) click();
    } else if (record.at("state").at("open") != true) {
      prepare();
      if (semantic_->resolve(step.at("target"), snapshot()->ui).record.at("state").at("open") != true)
        context->MouseClick();
    }
  }
  else if (op == "close") {
    if (record.at("kind") == "combo") {
      const auto& open = record.at("state").at("open");
      if (!open.is_boolean()) throw ProtocolError("value_unavailable", "Combo open state is unavailable");
      if (open.get<bool>()) { input(); context->KeyPress(ImGuiKey_Escape); }
    } else if (record.at("kind") == "menu") {
      if (record.at("state").at("open") == true) click();
    } else if (record.at("kind") == "popup") {
      if (record.at("state").at("open") == true) { input(); context->KeyPress(ImGuiKey_Escape); }
    }
    else if (record.at("state").at("open") == true) click();
  } else if (op == "set_checked") {
    const auto& checked = record.at("state").at("selected");
    if (!checked.is_boolean()) throw ProtocolError("value_unavailable", "Checked state is not known");
    if (checked != step.at("value")) click();
  } else if (op == "edit") {
    if (step.contains("value") && record.at("kind") != "number" && record.at("kind") != "slider")
      throw ProtocolError("invalid_request", "Typed numeric editing requires a scalar or component");
    const std::string text = step.contains("text") ? step.at("text").get<std::string>() : step.at("value").dump();
    if (text.find('\0') != std::string::npos)
      throw ProtocolError("invalid_request", "ImGui text input cannot represent embedded NUL characters");
    if (text.size() > record.value("capacity_bytes", std::size_t{16384}))
      throw ProtocolError("limit_exceeded", "Replacement exceeds the widget's UTF-8 capacity");
    const auto commit = step.value("commit", "none");
    if (commit != "none" && !contains(record.at("commit").at("methods"), commit))
      throw ProtocolError("unsupported", "Input does not support that commit method");
    prepare();
    context->KeyDown(ImGuiMod_Ctrl); context->MouseClick(); context->KeyUp(ImGuiMod_Ctrl);
    check_control();
    context->KeyCharsReplace(text.c_str());
    check_control();
    if (commit != "none") context->KeyPress(commit == "enter" ? ImGuiKey_Enter : ImGuiKey_Tab);
  } else if (op == "key") {
    if (!record.contains("chords") || !contains(record.at("chords"), step.at("chord")))
      throw ProtocolError("unsupported", "Key chord is not advertised by this target");
    static const std::map<std::string, ImGuiKeyChord> chords{
        {"Enter", ImGuiKey_Enter}, {"Escape", ImGuiKey_Escape}, {"Tab", ImGuiKey_Tab},
        {"Shift+Tab", ImGuiMod_Shift | ImGuiKey_Tab}, {"Left", ImGuiKey_LeftArrow},
        {"Right", ImGuiKey_RightArrow}, {"Up", ImGuiKey_UpArrow}, {"Down", ImGuiKey_DownArrow},
        {"Home", ImGuiKey_Home}, {"End", ImGuiKey_End}, {"PageUp", ImGuiKey_PageUp},
        {"PageDown", ImGuiKey_PageDown}, {"Ctrl+Z", ImGuiMod_Ctrl | ImGuiKey_Z},
        {"Ctrl+Y", ImGuiMod_Ctrl | ImGuiKey_Y}, {"Ctrl+S", ImGuiMod_Ctrl | ImGuiKey_S},
        {"Ctrl+Shift+Z", ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z}};
    if (record.at("kind") == "window" || record.at("kind") == "child") {
      if (ImGui::GetCurrentContext()->NavWindow != target_window) {
        if (!policy.value("auto_focus", false))
          throw ProtocolError("state_conflict", "Key target window is not focused; focus explicitly or permit auto_focus");
        assist("focus", [&] { context->WindowFocus(window_reference); context->Yield(); });
      }
    }
    else if (record.at("state").at("active") != true) {
      throw ProtocolError("state_conflict", "Key target is not active; focus it explicitly");
    }
    check_control();
    input();
    context->KeyPress(chords.at(step.at("chord").get<std::string>()));
  } else if (op == "drag") {
    if (record.at("state").at("active") == true)
      throw ProtocolError("state_conflict", "Finish active input before starting a separate drag gesture");
    const auto facts = prepare();
    const auto start = ImGui::GetIO().MousePos;
    const float distance = facts.RectFull.GetWidth() * step.at("fraction").get<float>() *
        (step.at("direction") == "increase" ? 1.F : -1.F);
    const int frames = step.value("frames", 4);
    context->MouseDown(); check_control();
    for (int frame = 1; frame <= frames; ++frame) {
      // Cross the real drag threshold through movement, never by editing a
      // float or manufacturing ImGui's drag-distance state.
      context->MouseTeleportToPos({start.x + distance * static_cast<float>(frame) / static_cast<float>(frames), start.y});
      context->Yield(); check_control();
    }
    context->MouseUp(); check_control();
  } else if (op == "scroll") {
    if (step.contains("to")) {
      const auto destination = semantic_->resolve(step.at("to"), current->ui);
      if (destination.window != item.window)
        throw ProtocolError("invalid_request", "Scroll destination belongs to a different window");
      input(); context->ScrollToItem(ImGuiTestRef(destination.id), ImGuiAxis_Y, ImGuiTestOpFlags_NoFocusWindow);
    } else {
      auto* window = context->GetWindowByRef(window_reference);
      if (!window) throw ProtocolError("not_found", "Scroll window is no longer submitted");
      const float amount = window->InnerRect.GetHeight() * step.at("pages").get<float>() *
                           (step.at("direction") == "up" ? -1.F : 1.F);
      input(); context->ScrollTo(window_reference, ImGuiAxis_Y,
          std::clamp(window->Scroll.y + amount, 0.F, window->ScrollMax.y), ImGuiTestOpFlags_NoFocusWindow);
    }
  }
  if (context->IsError()) throw ProtocolError("engine_error", "Test Engine could not complete the UI interaction");
  return knownValue(true, "boolean");
}

void SessionController::releaseInput(ImGuiTestContext* context) {
  if (context->IsError()) throw ProtocolError("engine_error", "Engine cannot verify input release");
  for (int button = 0; button != ImGuiMouseButton_COUNT; ++button)
    if (ImGui::GetIO().MouseDown[button]) context->MouseUp(button);
  for (int key = ImGuiKey_NamedKey_BEGIN; key != ImGuiKey_NamedKey_END; ++key)
    if (ImGui::IsKeyDown(static_cast<ImGuiKey>(key))) context->KeyUp(key);
  if (ImGui::GetIO().KeyMods) context->KeyUp(ImGui::GetIO().KeyMods);
  context->Yield();
  if (context->IsError()) throw ProtocolError("engine_error", "No verified release frame");
  const auto& io = ImGui::GetIO();
  if (io.KeyMods) throw ProtocolError("engine_error", "Modifiers remain held");
  for (const bool down : io.MouseDown) if (down) throw ProtocolError("engine_error", "Mouse button remains held");
  for (int key = ImGuiKey_NamedKey_BEGIN; key != ImGuiKey_NamedKey_END; ++key)
    if (ImGui::IsKeyDown(static_cast<ImGuiKey>(key))) throw ProtocolError("engine_error", "Key remains held");
}

void SessionController::dispatch(ImGuiTestContext* context) {
  const auto batch = batch_;
  if (!batch || context->IsError()) return;
  try {
    const auto& requested = batch->request.at("steps");
    while (batch->steps.size() < requested.size() && batch_ == batch) {
      if (!batch->cancel_code.empty()) throw ProtocolError(batch->cancel_code, batch->cancel_message);
      if (Clock::now() >= batch->deadline) throw ProtocolError("timeout", "Batch deadline expired");
      const auto index = batch->steps.size();
      const auto& step = requested[index];
      const Json before = stamp();
      const auto policy_denials = snapshot()->policy_denials;
      batch->assistance = Json::array();
      batch->acting = true;
      batch->step_deadline = std::min(batch->deadline,
          Clock::now() + std::chrono::milliseconds(step.value("timeout_ms", 5000)));
      Json observed;
      if (step.at("op") == "assert" || step.at("op") == "wait_until") {
        for (;;) {
          bool passes{};
          observed = evaluate(step.at("condition"), passes);
          if (passes) { batch->failure = nullptr; break; }
          {
            const bool waiting = step.at("op") == "wait_until";
            const bool unavailable = observed.value("availability", "unknown") != "known";
            auto failure = error(batch->request, unavailable ? "value_unavailable" : waiting ? "timeout" : "assertion_failed",
                                 "Observed value does not satisfy the condition", index);
            failure["condition"] = step.at("condition");
            failure["source"] = step.at("condition").at("source");
            failure["field"] = step.at("condition").at("field");
            failure["expected"] = availableAtom(step.at("condition").at("expected"));
            failure["observed"] = observed; batch->failure = std::move(failure);
          }
          if (observed.value("availability", "unknown") != "known")
            throw ProtocolError("value_unavailable", "Condition requires a complete known value");
          if (step.at("op") == "assert")
            throw ProtocolError("assertion_failed", "Observed value does not satisfy the condition");
          if (Clock::now() >= batch->step_deadline) throw ProtocolError("timeout", "Condition deadline expired");
          context->Yield();
          if (batch_ != batch) return;
          if (context->IsError()) throw ProtocolError("engine_error", "Engine stopped while waiting");
          if (!batch->cancel_code.empty()) throw ProtocolError(batch->cancel_code, batch->cancel_message);
        }
      } else {
        observed = action(context, step);
        const auto old_frame = frame_sequence_;
        do { context->Yield(); } while (!context->IsError() && batch_ == batch && frame_sequence_ <= old_frame);
        if (batch_ != batch) return;
        if (context->IsError()) throw ProtocolError("engine_error", "Engine stopped before completed action evidence");
        if (step.at("op") == "open" || step.at("op") == "close") {
          const auto& record = semantic_->resolve(step.at("target"), snapshot()->ui).record;
          if (record.at("state").at("open") != (step.at("op") == "open"))
            throw ProtocolError("state_conflict", "Widget did not reach the requested open state");
        }
        if (step.at("op") == "set_checked") {
          const auto& record = semantic_->resolve(step.at("target"), snapshot()->ui).record;
          if (record.at("state").at("selected") != step.at("value"))
            throw ProtocolError("state_conflict", "Checkbox did not reach the requested checked state");
        }
        if (snapshot()->policy_denials != policy_denials) {
          std::string denial = "Editor policy denied the requested effect";
          for (const auto& value : snapshot()->application)
            if (value.at("projection") == "diagnostics" && value.at("field") == "message" &&
                value.at("value").value("availability", "") == "known") {
              const auto text = value.at("value").at("value").get<std::string>();
              if (text.starts_with("policy_denied")) denial = text;
            }
          throw ProtocolError("policy_denied", denial);
        }
      }
      batch->acting = false;
      if (!batch->cancel_code.empty()) throw ProtocolError(batch->cancel_code, batch->cancel_message);
      if (Clock::now() >= batch->step_deadline) throw ProtocolError("timeout", "Operation deadline expired");
      batch->steps.push_back({{"index", index}, {"status", "passed"}, {"before", before},
                             {"after", stamp()}, {"observed", std::move(observed)},
                             {"assistance", batch->assistance}});
    }
    releaseInput(context);
    if (batch_ == batch) {
      if (!batch->cancel_code.empty()) finishBatch(batch->cancel_code, batch->cancel_message);
      else if (Clock::now() >= batch->deadline) finishBatch("timeout", "Batch deadline expired during cleanup");
      else finishBatch();
    }
  } catch (const ProtocolError& exception) {
    if (batch_ != batch) return;
    bool released = false;
    try { releaseInput(context); released = true; } catch (...) {}
    if (batch_ != batch) return;
    if (!released || exception.code() == "engine_error") faulted_ = closing_ = true;
    finishBatch(exception.code(), exception.what(), released);
  } catch (const std::exception& exception) {
    if (batch_ != batch) return;
    bool released = false;
    try { releaseInput(context); released = true; } catch (...) {}
    if (batch_ != batch) return;
    faulted_ = closing_ = true;
    finishBatch("engine_error", exception.what(), released);
  }
}

void SessionController::retainResult(const Json& request, const Json& result) {
  const auto id = request.at("request_id").get<std::string>();
  results_[id] = {request, result}; result_order_.push_back(id);
  while (result_order_.size() > 32) { results_.erase(result_order_.front()); result_order_.pop_front(); }
}

void SessionController::finishBatch(std::string_view code, std::string_view message, bool verified_cleanup) {
  const auto batch = batch_;
  if (!batch) return;
  auto result = base(batch->request);
  const auto count = batch->request.at("steps").size();
  const auto completed = batch->steps.size();
  const Json last = completed == 0 ? Json(nullptr) : Json(completed - 1);
  const bool success = code.empty() && verified_cleanup && completed == count;
  const Json failed = !success && verified_cleanup && completed < count ? Json(completed) : Json(nullptr);
  const std::string effects = !verified_cleanup ? "unknown" : batch->effects ? "partial" : "none";
  const std::string cleanup = verified_cleanup ? "released" : "unverified";
  const std::string state = faulted_ ? "faulted" : closing_ ? "closing" : "ready";
  result.update({{"ok", success}, {"steps", batch->steps}, {"before", batch->before},
      {"after", stamp(!verified_cleanup)}, {"last_completed_index", last}, {"failed_index", failed},
      {"effects", effects}, {"cleanup", cleanup}, {"session_state", state}});
  Json failure;
  if (!success) {
    failure = batch->failure.is_null() ? error(batch->request, code.empty() ? "engine_error" : code,
        message, failed.is_null() ? std::optional<std::size_t>{} : std::optional<std::size_t>{completed},
        completed < count ? batch->request["steps"][completed].value("target", Json(nullptr)) : Json(nullptr)) : batch->failure;
    failure.update({{"step_index", failed}, {"last_completed_index", last}, {"effects", effects},
                    {"cleanup", cleanup}, {"session_state", state},
                    {"code", code.empty() ? "engine_error" : code}, {"message", std::string(message.substr(0, 512))}});
    // A predicate's observed value belongs to its evaluation snapshot. Cleanup
    // yields another frame (and can commit/advance previews); result.after is
    // the final evidence, never a replacement timestamp for that earlier value.
    if (!verified_cleanup && !failure["snapshot"].is_null()) failure["snapshot"]["stale"] = true;
    if (completed < count && batch->request["steps"][completed].contains("condition")) {
      const auto& condition = batch->request["steps"][completed]["condition"];
      failure["condition"] = condition; failure["source"] = condition.at("source");
      failure["field"] = condition.at("field");
      failure["expected"] = availableAtom(condition.at("expected"));
    }
    result["error"] = failure;
  }
  for (std::size_t index = completed; index != count; ++index) {
    if (index == completed && !verified_cleanup) result["steps"].push_back(missingStep(index, "unknown"));
    else if (index == completed && !failed.is_null())
      result["steps"].push_back({{"index", index}, {"status", "failed"},
          {"before", nullptr}, {"after", stamp()}, {"error", failure}, {"assistance", batch->assistance}});
    else result["steps"].push_back(missingStep(index));
  }
  last_request_ = {{"request_id", batch->request.at("request_id")},
                   {"status", success ? "passed" : "failed"}, {"error", success ? Json(nullptr) : failure}};
  retainResult(batch->request, result);
  channel_.executionFinished(session_id_, batch->request.at("request_id").get<std::string>());
  batch_.reset(); activity_ = Clock::now();
  if (!channel_.closed()) reply("ui_execute", std::move(result));
}
}  // namespace editor_automation
