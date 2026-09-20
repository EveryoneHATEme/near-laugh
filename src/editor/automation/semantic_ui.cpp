#include "editor/automation/semantic_ui.hpp"

#include <imgui_internal.h>
#include <imgui_te_engine.h>
#include <imgui_te_internal.h>

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace editor_automation {
namespace {
thread_local SemanticUi* current_ui{};
struct Identity {
  std::string scope, key, owner, row;
  auto operator<=>(const Identity&) const = default;
};
struct Structure {
  Json values;
  std::uint64_t epoch{};
};
// The pinned InfoTask query only requests hook data: it does not yield, focus,
// scroll, activate an item, or use the clipboard. A newly registered item may
// need another completed frame before its task has current results. Never use
// ItemInfo()/ItemReadAs* here or manufacture missing hook results.
void observeEngineFacts(SemanticItem& item, const ImGuiContext& context) {
  auto* engine = static_cast<ImGuiTestEngine*>(context.TestEngine);
  item.record["engine"] = {{"availability", "unknown"},
      {"reason", !engine ? "Test Engine is not attached" :
                  !item.id ? "Widget has no Test Engine item ID" :
                             "Current-frame Test Engine item facts are unavailable"}};
  if (!engine || !item.id) return;
  const auto* facts = ImGuiTestEngine_FindItemInfo(engine, item.id, nullptr);
  if (!facts || facts->ID != item.id || facts->TimestampMain != context.FrameCount)
    return;

  item.engine_geometry = EngineItemGeometry{facts->RectFull.Min, facts->RectFull.Max,
      facts->RectClipped.Min, facts->RectClipped.Max, facts->TimestampMain};
  item.record["engine"] = {{"availability", "known"},
                           {"frame", std::to_string(facts->TimestampMain)}};
  item.record["state"]["submitted"] = true;
  item.record["state"]["enabled"] = !(facts->ItemFlags & ImGuiItemFlags_Disabled);
  item.record["provenance"]["state"] = "test_engine";

  if (facts->TimestampStatus != context.FrameCount) {
    item.record["engine"]["reason"] =
        "Current geometry and item flags; status flags unavailable, retaining UI metadata";
    return;
  }
  item.record["engine"]["status_frame"] = std::to_string(facts->TimestampStatus);
  item.record["state"]["visible"] = (facts->StatusFlags & ImGuiItemStatusFlags_Visible) != 0;
  if (facts->StatusFlags & ImGuiItemStatusFlags_Checkable)
    item.record["state"]["selected"] = (facts->StatusFlags & ImGuiItemStatusFlags_Checked) != 0;
  if (facts->StatusFlags & ImGuiItemStatusFlags_Openable)
    item.record["state"]["open"] = (facts->StatusFlags & ImGuiItemStatusFlags_Opened) != 0;
}
std::vector<EditorObjectId> objectIds(const EditorDocument& document) {
  std::vector<EditorObjectId> ids;
  const auto append = [&](const auto& values) {
    ids.insert(ids.end(), values.begin(), values.end());
  };
  append(document.solidIds()); append(document.entryIds());
  append(document.lightIds()); append(document.switchIds());
  append(document.doorIds()); append(document.propIds());
  for (int kind = 0; kind != 4; ++kind)
    append(document.audioIds(static_cast<EditorAudioKind>(kind)));
  for (int kind = 0; kind != 3; ++kind) {
    append(document.characterIds(static_cast<EditorCharacterKind>(kind)));
    append(document.householdIds(static_cast<EditorHouseholdKind>(kind)));
  }
  return ids;
}
}  // namespace

std::string semanticKey(std::string_view label) {
  if (const auto hidden = label.find("###"); hidden != std::string_view::npos)
    label.remove_prefix(hidden + 3);
  else if (const auto hidden = label.find("##"); hidden != std::string_view::npos)
    label = label.substr(0, hidden);
  std::string result;
  for (unsigned char value : label) {
    if (result.size() == 96) break;
    if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
    if ((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9'))
      result += static_cast<char>(value);
    else if (!result.empty() && result.back() != '-') result += '-';
  }
  if (!result.empty() && result.back() == '-') result.pop_back();
  return result.empty() ? "item" : result;
}

struct SemanticUi::Impl {
  explicit Impl(std::string id)
      : prefix("s" + std::move(id)) {}
  std::string prefix;
  std::uint64_t serial{}, generation{};
  bool has_generation{}, in_frame{}, truncated{};
  ObjectRefs refs;
  std::map<Identity, std::string> identities;
  std::map<std::pair<std::string, std::string>, Structure> structures;
  std::vector<EditorObjectId> owners;
  std::vector<std::string> rows;
  struct Scope { std::string path, ref, window; };
  std::vector<Scope> scopes;
  std::map<std::string, std::string> scope_parents, scope_items, window_scopes;
  std::map<std::string, std::pair<std::string, std::string>> row_records;
  std::vector<SemanticItem> items;
  std::map<std::string, std::set<std::string>> limitations;
  std::string next_key, next_field, next_unit, next_projection;
  bool next_immediate{};

  std::string newRef(const char* kind) {
    return prefix + "-" + kind + std::to_string(++serial);
  }
  std::string owner() const {
    if (owners.empty()) return {};
    const auto found = refs.find(owners.back());
    return found == refs.end() ? std::string{} : found->second;
  }
  void synchronize(const EditorDocument& document) {
    if (!has_generation || generation != document.generation()) {
      refs.clear(); structures.clear(); identities.clear();
      generation = document.generation(); has_generation = true;
    }
    const auto ids = objectIds(document);
    const std::set<EditorObjectId> live(ids.begin(), ids.end());
    std::erase_if(refs, [&](const auto& value) { return !live.contains(value.first); });
    for (const auto id : ids)
      if (!refs.contains(id)) refs.emplace(id, newRef("o"));
    std::set<std::string> live_refs;
    for (const auto& [id, ref] : refs) {
      live_refs.insert(ref);
      const auto value = document.object(id);
      if (!value) continue;
      const auto structure = [&](const char* family, Json values) {
        auto [entry, added] = structures.try_emplace({ref, family}, Structure{values, 0});
        if (!added) {
          // Index-only rows have no stable identity of their own. Invalidate on
          // any array change, including replacement combined with reordering;
          // unrelated properties of the owner retain their references.
          if (entry->second.values != values) ++entry->second.epoch;
          entry->second.values = std::move(values);
        }
      };
      if (const auto* prop = std::get_if<PrototypeStaticProp>(&*value)) {
        Json boxes = Json::array();
        for (const auto& box : prop->collision_boxes)
          boxes.push_back({box.center.x, box.center.y, box.center.z,
                           box.half_extent.x, box.half_extent.y, box.half_extent.z});
        structure("collision_boxes", std::move(boxes));
      } else if (const auto* readable = std::get_if<HouseholdDocumentDefinition>(&*value))
        structure("pages", readable->pages);
      else if (const auto* route = std::get_if<CharacterRouteDefinition>(&*value))
        structure("marks", route->marks);
    }
    std::erase_if(structures, [&](const auto& entry) {
      return !live_refs.contains(entry.first.first);
    });
    std::erase_if(identities, [&](const auto& entry) {
      return !entry.first.owner.empty() && !live_refs.contains(entry.first.owner);
    });
  }
};

SemanticUi::SemanticUi(std::string session_id)
    : impl_(std::make_unique<Impl>(std::move(session_id))) {}
SemanticUi::~SemanticUi() { if (current_ui == this) current_ui = nullptr; }
SemanticUi* SemanticUi::active() noexcept { return current_ui; }

void SemanticUi::beginFrame(const EditorDocument& document) {
  if (current_ui || impl_->in_frame)
    throw ProtocolError("state_conflict", "A semantic UI frame is already active");
  impl_->synchronize(document);
  impl_->items.clear(); impl_->owners.clear(); impl_->rows.clear();
  impl_->scopes.clear(); impl_->limitations.clear();
  impl_->scope_parents = {{"root", ""}}; impl_->scope_items.clear();
  impl_->window_scopes.clear(); impl_->row_records.clear();
  impl_->next_key.clear(); impl_->next_field.clear(); impl_->next_unit.clear();
  impl_->next_immediate = false;
  impl_->truncated = false;
  impl_->in_frame = true;
  current_ui = this;
}

SemanticSnapshot SemanticUi::finishFrame(const EditorDocument& document) {
  if (current_ui != this || !impl_->in_frame)
    throw ProtocolError("state_conflict", "No semantic UI frame is active");
  current_ui = nullptr;
  impl_->in_frame = false;
  impl_->synchronize(document);
  SemanticSnapshot result;
  result.object_refs = impl_->refs;

  if (const auto selected = impl_->refs.find(document.selection()); selected != impl_->refs.end())
    result.selection_owner = selected->second;
  const ImGuiContext& context = *ImGui::GetCurrentContext();
  const ImGuiWindow* modal = ImGui::GetTopMostPopupModal();
  // Built-in auxiliary UI (for example ColorEdit's context menu) can submit
  // controls outside our wrappers. Preserve its scope and report the gap instead
  // of making an absence/completeness claim about those controls.
  for (const auto* window : context.Windows) {
    if (!window->Active || !(window->Flags & ImGuiWindowFlags_Popup) ||
        impl_->window_scopes.contains(window->Name)) continue;
    if (impl_->items.size() == 4096) { impl_->truncated = true; break; }
    const Identity identity{"$scope", std::string("unregistered/") + window->Name, "", ""};
    auto [entry, added] = impl_->identities.try_emplace(identity);
    if (added) entry->second = impl_->newRef("c");
    const auto ref = entry->second;
    const auto parent = window->ParentWindow ? impl_->window_scopes.find(window->ParentWindow->Name) : impl_->window_scopes.end();
    impl_->scope_parents[ref] = parent == impl_->window_scopes.end() ? "root" : parent->second;
    impl_->scope_items[ref] = ref; impl_->window_scopes[window->Name] = ref;
    impl_->limitations[ref].insert("unregistered");
    const auto absent = unavailableValue("Auxiliary ImGui controls have no adapter", "unavailable");
    Json record{{"ref", ref}, {"scope", ref}, {"key", "unregistered-popup"}, {"owner", nullptr},
      {"label", "Auxiliary ImGui popup"}, {"kind", "popup"}, {"capabilities", {"assert"}},
      {"state", {{"submitted", true}, {"visible", !window->Hidden}, {"enabled", absent},
                   {"active", false}, {"focused", context.NavWindow == window},
                   {"selected", absent}, {"open", true}}}, {"value", absent}, {"input", absent},
      {"commit", {{"policy", "not_applicable"}, {"methods", Json::array()}}},
      {"applied_binding", nullptr}, {"unsupported_reason", "Auxiliary controls are unregistered"},
      {"provenance", {{"identity", "ui_metadata"}, {"state", "imgui"}, {"value", "unavailable"}}}};
    impl_->items.push_back({std::move(record), window->ID, window->Name});
  }
  result.scope_parents = impl_->scope_parents;
  result.scope_items = impl_->scope_items;
  if (context.NavWindow && impl_->window_scopes.contains(context.NavWindow->Name))
    result.active_scope = impl_->window_scopes.at(context.NavWindow->Name);
  if (modal && impl_->window_scopes.contains(modal->Name))
    result.modal_scope = impl_->window_scopes.at(modal->Name);
  std::set<std::string> live;
  for (const auto& [id, ref] : impl_->refs) { (void)id; live.insert(ref); }
  for (auto& item : impl_->items) {
    if (!item.record["owner"].is_null() &&
        !live.contains(item.record["owner"].get<std::string>())) continue;
    if (const auto row = impl_->row_records.find(item.record["ref"].get<std::string>()); row != impl_->row_records.end()) {
      const auto& token = row->second.second;
      const auto family = token.substr(0, token.find(':'));
      const auto structure = impl_->structures.find({row->second.first, family});
      const auto recorded = token.substr(token.find(':') + 1, token.rfind(':') - token.find(':') - 1);
      if (structure != impl_->structures.end() && recorded != std::to_string(structure->second.epoch)) {
        impl_->limitations[item.record["scope"].get<std::string>()].insert("unregistered");
        continue;
      }
    }
    if (item.id) {
      item.record["state"]["active"] = context.ActiveId == item.id;
      item.record["state"]["focused"] = context.NavId == item.id;
    }
    observeEngineFacts(item, context);
    if (context.ActiveId && context.ActiveId == item.id) result.active_scope = item.record["scope"];
    result.items.push_back(std::move(item));
  }
  Json scopes = Json::array();
  std::set<std::string> limitations{"submitted_only"};
  for (const auto& [scope, values] : impl_->limitations) {
    if (scopes.size() < 256) scopes.push_back({{"scope", scope}, {"limitations", values}});
    limitations.insert(values.begin(), values.end());
  }
  if (impl_->truncated) limitations.insert("truncation");
  result.coverage = {{"limitations", limitations},
                     {"snapshot_item_count", result.items.size()},
                     {"truncated", impl_->truncated}, {"scopes", std::move(scopes)}};
  return result;
}

const SemanticItem& SemanticUi::resolve(const Json& target,
                                         const SemanticSnapshot& snapshot) const {
  const SemanticItem* found{};
  for (const auto& item : snapshot.items) {
    bool matches{};
    if (target.contains("ref")) matches = item.record["ref"] == target["ref"];
    else if (target.contains("selector")) {
      const auto& selector = target["selector"];
      matches = item.record["scope"] == selector.at("scope") &&
                item.record["key"] == selector.at("key");
      if (matches && selector.contains("owner")) {
        const auto owner = selector["owner"] == "selection"
                               ? Json(snapshot.selection_owner) : selector["owner"];
        matches = item.record["owner"] == owner;
      }
    } else throw ProtocolError("invalid_request", "Target needs a ref or exact selector");
    if (matches) {
      if (found) throw ProtocolError("ambiguous_target", "Exact target matches several submitted items");
      found = &item;
    }
  }
  if (!found) throw ProtocolError(target.contains("ref") ? "stale_ref" : "not_found",
                                  "Target is not present in this UI snapshot");
  return *found;
}

void SemanticUi::pushOwner(EditorObjectId owner) { impl_->owners.push_back(owner); }
void SemanticUi::popOwner() { if (!impl_->owners.empty()) impl_->owners.pop_back(); }
bool SemanticUi::hasOwner() const { return !impl_->owner().empty(); }
void SemanticUi::pushRow(std::string_view family, std::size_t index) {
  const auto found = impl_->structures.find({impl_->owner(), std::string(family)});
  const auto epoch = found == impl_->structures.end() ? 0 : found->second.epoch;
  impl_->rows.push_back(std::string(family) + ":" + std::to_string(epoch) + ":" + std::to_string(index));
}
void SemanticUi::popRow() { if (!impl_->rows.empty()) impl_->rows.pop_back(); }
void SemanticUi::structureChanged(std::string_view family) {
  const auto owner = impl_->owner();
  if (!owner.empty()) ++impl_->structures[{owner, std::string(family)}].epoch;
}
void SemanticUi::next(std::string_view key, std::string_view field,
                       std::string_view unit, std::string_view projection,
                       bool immediate) {
  impl_->next_key = key; impl_->next_field = field;
  impl_->next_unit = unit; impl_->next_projection = projection;
  impl_->next_immediate = immediate;
}
std::string SemanticUi::scope() const {
  return impl_->scopes.empty() ? "root" : impl_->scopes.back().ref;
}
void SemanticUi::pushScope(std::string_view key, std::string_view window, bool root) {
  if (impl_->scopes.size() >= 16)
    throw ProtocolError("limit_exceeded", "Semantic scope depth exceeded");
  const auto parent = root ? std::string("root") : scope();
  const auto path = (root || impl_->scopes.empty() ? std::string{} : impl_->scopes.back().path + "/") + std::string(key);
  const Identity identity{"$scope", path, impl_->owner(), impl_->rows.empty() ? "" : impl_->rows.back()};
  auto [entry, created] = impl_->identities.try_emplace(identity);
  if (created) entry->second = impl_->newRef("c");
  impl_->scope_parents[entry->second] = parent;
  impl_->window_scopes[std::string(window)] = entry->second;
  impl_->scopes.push_back({path, entry->second, std::string(window)});
}
void SemanticUi::scopeItem(std::string_view ref) { impl_->scope_items[scope()] = ref; }

void SemanticUi::popScope() { if (!impl_->scopes.empty()) impl_->scopes.pop_back(); }
void SemanticUi::limitation(std::string_view reason) {
  impl_->limitations[scope()].insert(std::string(reason));
}

std::string SemanticUi::record(Json record, ImGuiID id, std::string_view window,
                                std::string_view default_key) {
  auto key = impl_->next_key.empty() ? std::string(default_key) : impl_->next_key;
  if (!impl_->rows.empty()) {
    const auto& row = impl_->rows.back();
    key = row.substr(0, row.find(':')) + "[" + row.substr(row.rfind(':') + 1) + "]/" + key;
  }
  const auto owner = impl_->owner();
  record["scope"] = scope(); record["key"] = key;
  record["owner"] = owner.empty() ? Json(nullptr) : Json(owner);
  if (!impl_->next_field.empty())
    record["applied_binding"] = {{"projection", impl_->next_projection}, {"field", impl_->next_field}};
  if (!impl_->next_unit.empty() && record["value"]["availability"] == "known")
    record["value"]["unit"] = impl_->next_unit;
  if (impl_->next_immediate) record["commit"]["policy"] = "immediate";
  impl_->next_key.clear(); impl_->next_field.clear(); impl_->next_unit.clear();
  impl_->next_immediate = false;
  if (impl_->items.size() == 4096) { impl_->truncated = true; return {}; }
  const Identity identity{scope(), key, owner, impl_->rows.empty() ? "" : impl_->rows.back()};
  std::string ref;
  if (record.value("ref_lifetime", "semantic") == "snapshot") ref = impl_->newRef("f");
  else {
    if (impl_->identities.size() >= 16384 && !impl_->identities.contains(identity))
      throw ProtocolError("limit_exceeded", "Session semantic identity storage is full");
    auto [entry, created] = impl_->identities.try_emplace(identity);
    if (created) entry->second = impl_->newRef("i");
    ref = entry->second;
  }
  const auto kind = record.value("kind", "");
  if (kind == "window" || kind == "popup" || kind == "child") {
    ref = scope();
    scopeItem(ref);
  }
  if (!impl_->rows.empty()) impl_->row_records[ref] = {owner, impl_->rows.back()};
  record["ref"] = ref;
  if (id)
    if (auto* engine = static_cast<ImGuiTestEngine*>(ImGui::GetCurrentContext()->TestEngine))
      // Renew before frame completion so the next ItemAdd/ItemInfo hooks collect
      // this actual item. No pointer to the engine record survives this call.
      static_cast<void>(ImGuiTestEngine_FindItemInfo(engine, id, nullptr));
  impl_->items.push_back({std::move(record), id, std::string(window)});
  return ref;
}
void SemanticUi::components(std::string_view parent, const Json& components) {
  for (auto& item : impl_->items)
    if (item.record["ref"] == Json(parent)) {
      item.record["components"] = components;
      const auto binding = item.record["applied_binding"];
      for (const auto& component : components) {
        for (auto& child : impl_->items) if (child.record["ref"] == component["ref"]) {
          if (item.record["commit"]["policy"] == "immediate")
            child.record["commit"]["policy"] = "immediate";
          if (!binding.is_null()) {
            auto field = binding["field"].get<std::string>();
            auto axis = component["key"].get<std::string>();
            if (field == "color" && axis.size() == 1 && std::string("xyz").find(axis) != std::string::npos)
              axis = std::string(1, "rgb"[std::string("xyz").find(axis)]);
            if (field != "collision_boxes") field += "." + axis;
            child.record["applied_binding"] = {{"projection", binding["projection"]}, {"field", field}};
          }
          if (item.record["value"].contains("unit") && child.record["value"]["availability"] == "known")
            child.record["value"]["unit"] = item.record["value"]["unit"];
        }
      }
      return;
    }
}
}  // namespace editor_automation
