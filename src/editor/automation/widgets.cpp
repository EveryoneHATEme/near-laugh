#include "editor/automation/widgets.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <string>

#include "editor/automation/semantic_ui.hpp"

namespace editor_automation::widgets {
namespace {
constexpr std::size_t text_limit = 16384;
Json absent(const char* reason = "Not applicable to this widget") {
  return unavailableValue(reason, "not_applicable");
}
Json draft(Json value, const char* type) {
  auto result = knownValue(std::move(value), type);
  if (result.contains("value")) {
    result["draft"] = std::move(result["value"]);
    result.erase("value");
  }
  return result;
}
Json numeric(float value) {
  return std::isfinite(value) ? draft(value, "float")
                             : unavailableValue("Numeric draft is not finite");
}
std::string bounded(std::string_view value) {
  auto size = std::min(value.size(), text_limit);
  if (size < value.size())
    while (size && (static_cast<unsigned char>(value[size]) & 0xc0) == 0x80) --size;
  return std::string(value.substr(0, size));
}
std::string labelText(const char* label) {
  std::string_view value(label ? label : "");
  return std::string(value.substr(0, value.find("##")));
}
Json inputValue(ImGuiID id, bool text_input, std::string_view fallback = {}) {
  const auto& context = *ImGui::GetCurrentContext();
  if (id && context.ActiveId == id) {
    if (auto* state = ImGui::GetInputTextState(id)) {
      const std::string_view buffer(state->GetText(), static_cast<std::size_t>(state->TextLen));
      const bool changed = state->TextToRevertTo.Data && buffer != state->TextToRevertTo.Data;
      Json result{{"availability", buffer.size() > text_limit ? "truncated" : "known"},
                  {"text", bounded(buffer)}, {"uncommitted", changed}};
      if (buffer.size() > text_limit) result["reason"] = "Active input exceeds the observation byte limit";
      return result;
    }
  }
  if (text_input) {
    Json result{{"availability", fallback.size() > text_limit ? "truncated" : "known"},
                {"text", bounded(fallback)}, {"uncommitted", false}};
    if (fallback.size() > text_limit) result["reason"] = "Inactive input exceeds the observation byte limit";
    return result;
  }
  return absent("Numeric input mode is inactive");
}
Json item(const char* label, const char* kind, ImGuiID id,
          Json capabilities, Json value = absent()) {
  auto& context = *ImGui::GetCurrentContext();
  const auto* window = ImGui::GetCurrentWindow();
  const bool last = id && context.LastItemData.ID == id;
  const bool enabled = !(context.CurrentItemFlags & ImGuiItemFlags_Disabled);
  Json visible = last ? Json(ImGui::IsItemVisible())
                      : unavailableValue("Visibility was not available at this registration", "unknown");
  const auto full_label = labelText(label);
  Json result{{"label", bounded(full_label)},
              {"label_availability", full_label.size() > text_limit ? "truncated" : "known"}, {"kind", kind},
              {"capabilities", std::move(capabilities)},
              {"state", {{"submitted", !window->SkipItems}, {"visible", std::move(visible)},
                           {"enabled", enabled}, {"active", id && context.ActiveId == id},
                           {"focused", id && context.NavId == id},
                           {"selected", absent()}, {"open", absent()}}},
              {"value", std::move(value)}, {"input", absent()},
              {"commit", {{"policy", "not_applicable"}, {"methods", Json::array()}}},
              {"applied_binding", nullptr},
              {"provenance", {{"identity", "ui_metadata"}, {"state", "imgui"}, {"value", "ui_draft"}}},
              {"ref_lifetime", "semantic"}};
  return result;
}
std::string submit(Json value, ImGuiID id, std::string_view key,
                    std::string_view window = {}) {
  auto* ui = SemanticUi::active();
  if (!ui) return {};
  if (value["state"]["submitted"] == false && value["kind"] != "window" && value["kind"] != "popup" && value["kind"] != "child") {
    ui->limitation("clipped_unsubmitted");
    // Consume a UI-local next annotation even when ImGui omitted its item.
    ui->next({});
    return {};
  }
  if (window.empty()) window = ImGui::GetCurrentWindow()->Name;
  return ui->record(std::move(value), id, window, key);
}
void numericInput(Json& value, ImGuiID id) {
  value["input"] = inputValue(id, false);
  value["input_validation"] = {{"status", "unknown"}, {"reason", "Numeric input mode is inactive"}};
  if (value["input"]["availability"] == "known") {
    const auto text = value["input"]["text"].get<std::string>();
    char* end{}; errno = 0;
    const auto parsed = std::strtof(text.c_str(), &end);
    while (end && (*end == ' ' || *end == '\t')) ++end;
    const bool valid = end != text.c_str() && end && !*end && errno != ERANGE && std::isfinite(parsed);
    // Syntax of the editable buffer is independent of the copied typed draft.
    value["input_validation"] = {{"status", valid ? "valid" : "invalid"},
        {"reason", valid ? "Finite numeric text; application validation is separate"
                         : "Active numeric text is incomplete or not finite"}};
  }
}
void commit(Json& value, const char* policy, Json methods) {
  value["commit"] = {{"policy", policy}, {"methods", std::move(methods)}};
}
void textRecord(std::string_view text, const char* key) {
  if (!SemanticUi::active()) return;
  auto value = item(std::string(text).c_str(), "diagnostic", 0,
                    {"assert", "wait_until"}, draft(std::string(text), "string"));
  value["ref_lifetime"] = "snapshot";
  value["state"]["visible"] = ImGui::IsItemVisible();
  value["state"]["active"] = false;
  value["state"]["focused"] = false;
  value["provenance"]["value"] = "ui_metadata";
  submit(std::move(value), 0, semanticKey(key));
}
std::string formatText(const char* format, va_list arguments) {
  std::array<char, text_limit + 5> buffer{};
  va_list copy; va_copy(copy, arguments);
  const int length = std::vsnprintf(buffer.data(), buffer.size(), format, copy);
  va_end(copy);
  return length < 0 ? std::string{} : std::string(buffer.data(), std::min(static_cast<std::size_t>(length), buffer.size() - 1));
}
void windowRecord(const char* name, const char* kind, bool open) {
  if (!SemanticUi::active()) return;
  auto* window = ImGui::GetCurrentWindow();
  auto value = item(name, kind, window->ID, {"focus", "scroll", "key", "assert"});
  value["state"]["visible"] = !window->Hidden && !window->Collapsed;
  value["state"]["open"] = open;
  value["chords"] = {"Enter", "Escape", "Tab", "Shift+Tab", "Left", "Right", "Up", "Down",
                      "Home", "End", "PageUp", "PageDown", "Ctrl+Z", "Ctrl+Y", "Ctrl+S", "Ctrl+Shift+Z"};
  submit(std::move(value), window->ID, "window");
  if (!open) SemanticUi::active()->limitation("collapsed");
}
void numberRecord(const char* label, const char* kind, ImGuiID id, float number,
                    float low, float high, bool enter_only = false, bool draggable = false) {
  if (!SemanticUi::active()) return;
  auto value = item(label, kind, id, {"focus", "edit", "commit", "key", "assert"}, numeric(number));
  numericInput(value, id);
  if (draggable) {
    value["capabilities"].push_back("drag");
    value["drag"] = {{"axis", "horizontal"}, {"distance_unit", "widget_width"}};
  }
  value["capacity_bytes"] = 63;
  value["chords"] = {"Enter", "Escape", "Tab", "Shift+Tab", "Left", "Right", "Home", "End"};
  commit(value, enter_only ? "enter" : "deactivate", enter_only ? Json{"enter"} : Json{"enter", "tab"});
  if (low < high && std::isfinite(low) && std::isfinite(high)) value["range"] = {{"min", low}, {"max", high}};
  submit(std::move(value), id, semanticKey(label));
}
void textInputRecord(const char* label, ImGuiID id, const char* buffer,
                       std::size_t capacity, bool multiline, ImGuiInputTextFlags flags) {
  if (!SemanticUi::active()) return;
  auto value = item(label, multiline ? "multiline_text" : "text", id,
                    {"focus", "edit", "commit", "key", "assert"}, draft(buffer, "string"));
  value["input"] = inputValue(id, true, buffer);
  value["capacity_bytes"] = std::min(capacity ? capacity - 1 : 0, text_limit);
  value["chords"] = {"Enter", "Escape", "Tab", "Shift+Tab", "Left", "Right", "Up", "Down", "Home", "End"};
  const bool enter = (flags & ImGuiInputTextFlags_EnterReturnsTrue) != 0;
  commit(value, enter ? "enter" : "deactivate",
          multiline ? Json{"tab"} : enter ? Json{"enter"} : Json{"enter", "tab"});
  submit(std::move(value), id, semanticKey(label));
}
}  // namespace

bool Begin(const char* name, bool* open, ImGuiWindowFlags flags) {
  const bool result = ImGui::Begin(name, open, flags);
  if (auto* ui = SemanticUi::active()) ui->pushScope(semanticKey(name), name, true);
  windowRecord(name, "window", result);
  return result;
}
void End() { ImGui::End(); if (auto* ui = SemanticUi::active()) ui->popScope(); }
bool BeginChild(const char* id, const ImVec2& size, ImGuiChildFlags child_flags,
                  ImGuiWindowFlags window_flags) {
  const bool result = ImGui::BeginChild(id, size, child_flags, window_flags);
  if (auto* ui = SemanticUi::active()) ui->pushScope(semanticKey(id), ImGui::GetCurrentWindow()->Name);
  windowRecord(id, "child", result);
  return result;
}
void EndChild() { ImGui::EndChild(); if (auto* ui = SemanticUi::active()) ui->popScope(); }
bool BeginMainMenuBar() {
  const bool result = ImGui::BeginMainMenuBar();
  if (result) {
    if (auto* ui = SemanticUi::active()) ui->pushScope("menu-bar", ImGui::GetCurrentWindow()->Name, true);
    windowRecord("Menu bar", "window", true);
  }
  return result;
}
void EndMainMenuBar() { ImGui::EndMainMenuBar(); if (auto* ui = SemanticUi::active()) ui->popScope(); }
bool BeginMenu(const char* label, bool enabled) {
  auto* before = ImGui::GetCurrentWindow();
  const auto id = before->GetID(label);
  const std::string name = before->Name;
  const bool effective_enabled = enabled && !(ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled);
  const bool result = ImGui::BeginMenu(label, enabled);
  if (auto* ui = SemanticUi::active()) {
    auto value = item(label, "menu", id, {"open", "close", "focus", "assert"});
    value["state"]["open"] = result; value["state"]["enabled"] = effective_enabled;
    const auto ref = submit(std::move(value), id, semanticKey(label), name);
    if (result) { ui->pushScope("menu-" + semanticKey(label), ImGui::GetCurrentWindow()->Name); ui->scopeItem(ref); }
    else ui->limitation("closed_popup");
  }
  return result;
}
void EndMenu() { ImGui::EndMenu(); if (auto* ui = SemanticUi::active()) ui->popScope(); }
bool BeginCombo(const char* label, const char* preview, ImGuiComboFlags flags) {
  auto* before = ImGui::GetCurrentWindow();
  const auto id = before->GetID(label);
  const std::string name = before->Name;
  const bool enabled = !(ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled);
  const bool result = ImGui::BeginCombo(label, preview, flags);
  if (auto* ui = SemanticUi::active()) {
    auto value = item(label, "combo", id, {"open", "close", "focus", "assert"},
                      preview ? draft(preview, "string") : absent());
    value["state"]["open"] = result; value["state"]["enabled"] = enabled;
    commit(value, "immediate", Json::array());
    const auto ref = submit(std::move(value), id, semanticKey(label), name);
    if (result) { ui->pushScope("combo-" + semanticKey(label), ImGui::GetCurrentWindow()->Name); ui->scopeItem(ref); }
    else ui->limitation("closed_popup");
  }
  return result;
}
void EndCombo() { ImGui::EndCombo(); if (auto* ui = SemanticUi::active()) ui->popScope(); }
bool BeginPopupModal(const char* name, bool* open, ImGuiWindowFlags flags) {
  const bool result = ImGui::BeginPopupModal(name, open, flags);
  if (auto* ui = SemanticUi::active()) {
    if (result) {
      ui->pushScope(semanticKey(name), ImGui::GetCurrentWindow()->Name, true);
      windowRecord(name, "popup", true);
    } else ui->limitation("closed_popup");
  }
  return result;
}
void EndPopup() { ImGui::EndPopup(); if (auto* ui = SemanticUi::active()) ui->popScope(); }
bool Button(const char* label, const ImVec2& size) {
  const auto id = ImGui::GetID(label);
  const bool result = ImGui::Button(label, size);
  if (SemanticUi::active()) submit(item(label, "button", id, {"activate", "focus", "assert"}), id, semanticKey(label));
  return result;
}
bool MenuItem(const char* label, const char* shortcut, bool selected, bool enabled) {
  const auto id = ImGui::GetID(label);
  const bool effective_enabled = enabled && !(ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled);
  const bool result = ImGui::MenuItem(label, shortcut, selected, enabled);
  if (SemanticUi::active()) {
    auto value = item(label, "menu_item", id, {"activate", "focus", "assert"}, draft(selected, "boolean"));
    value["state"]["selected"] = selected; value["state"]["enabled"] = effective_enabled;
    commit(value, "immediate", Json::array());
    submit(std::move(value), id, semanticKey(label));
  }
  return result;
}
bool Checkbox(const char* label, bool* boolean) {
  const auto id = ImGui::GetID(label);
  const bool result = ImGui::Checkbox(label, boolean);
  if (SemanticUi::active()) {
    auto value = item(label, "checkbox", id, {"set_checked", "activate", "focus", "assert"}, draft(*boolean, "boolean"));
    value["state"]["selected"] = *boolean;
    commit(value, "immediate", Json::array());
    submit(std::move(value), id, semanticKey(label));
  }
  return result;
}
bool Selectable(const char* label, bool selected, ImGuiSelectableFlags flags,
                  const ImVec2& size) {
  const auto id = ImGui::GetID(label);
  const bool result = ImGui::Selectable(label, selected, flags, size);
  if (SemanticUi::active()) {
    auto value = item(label, "selectable", id, {"select", "activate", "focus", "assert"}, draft(selected, "boolean"));
    value["state"]["enabled"] = value["state"]["enabled"].get<bool>() && !(flags & ImGuiSelectableFlags_Disabled);
    value["state"]["selected"] = selected;
    commit(value, "immediate", Json::array());
    submit(std::move(value), id, semanticKey(label));
  }
  return result;
}
bool Combo(const char* label, int* current, const char* separated, int height) {
  if (height != -1 && !(ImGui::GetCurrentContext()->NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
    const float maximum = (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.y) * height - ImGui::GetStyle().ItemSpacing.y + ImGui::GetStyle().WindowPadding.y * 2;
    ImGui::SetNextWindowSizeConstraints({0, 0}, {FLT_MAX, maximum});
  }
  const auto combo_id = ImGui::GetID(label);
  std::vector<const char*> choices;
  for (const char* option = separated; *option; option += std::strlen(option) + 1) choices.push_back(option);
  const char* preview = *current >= 0 && static_cast<std::size_t>(*current) < choices.size() ? choices[*current] : "";
  bool changed{};
  if (widgets::BeginCombo(label, preview)) {
    for (std::size_t index = 0; index < choices.size(); ++index) {
      ImGui::PushID(static_cast<int>(index));
      if (auto* ui = SemanticUi::active()) ui->next("option-" + std::to_string(index));
      const bool selected = *current == static_cast<int>(index);
      if (widgets::Selectable(choices[index], selected)) {
        *current = static_cast<int>(index); changed = true;
      }
      if (selected) ImGui::SetItemDefaultFocus();
      ImGui::PopID();
    }
    widgets::EndCombo();
    if (changed) ImGui::MarkItemEdited(combo_id);
  }
  return changed;
}
bool CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags) {
  const auto id = ImGui::GetID(label);
  const bool result = ImGui::CollapsingHeader(label, flags);
  if (auto* ui = SemanticUi::active()) {
    auto value = item(label, "section", id, {"open", "close", "assert"}, draft(result, "boolean"));
    value["state"]["open"] = result;
    submit(std::move(value), id, semanticKey(label));
    if (!result) ui->limitation("collapsed");
  }
  return result;
}
bool InputText(const char* label, char* buffer, size_t capacity, ImGuiInputTextFlags flags,
                 ImGuiInputTextCallback callback, void* user_data) {
  const auto id = ImGui::GetID(label);
  const bool result = ImGui::InputText(label, buffer, capacity, flags, callback, user_data);
  textInputRecord(label, id, buffer, capacity, false, flags);
  return result;
}
bool InputTextMultiline(const char* label, char* buffer, size_t capacity, const ImVec2& size,
                          ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
  const auto id = ImGui::GetID(label);
  const bool result = ImGui::InputTextMultiline(label, buffer, capacity, size, flags, callback, user_data);
  textInputRecord(label, id, buffer, capacity, true, flags);
  return result;
}
bool InputFloat(const char* label, float* value, float step, float fast_step,
                  const char* format, ImGuiInputTextFlags flags) {
  const auto id = ImGui::GetID(label);
  const bool result = ImGui::InputFloat(label, value, step, fast_step, format, flags);
  numberRecord(label, "number", id, *value, 0, 0, (flags & ImGuiInputTextFlags_EnterReturnsTrue) != 0);
  return result;
}
bool DragFloat(const char* label, float* value, float speed, float low, float high,
                 const char* format, ImGuiSliderFlags flags) {
  const auto id = ImGui::GetID(label);
  const bool result = ImGui::DragFloat(label, value, speed, low, high, format, flags);
  numberRecord(label, "number", id, *value, low, high, false, true);
  return result;
}
bool SliderFloat(const char* label, float* value, float low, float high,
                   const char* format, ImGuiSliderFlags flags) {
  const auto id = ImGui::GetID(label);
  const bool result = ImGui::SliderFloat(label, value, low, high, format, flags);
  numberRecord(label, "slider", id, *value, low, high, false, true);
  return result;
}
bool DragFloat3(const char* label, float values[3], float speed, float low, float high,
                  const char* format, ImGuiSliderFlags flags) {
  const auto parent_id = ImGui::GetID(label);
  const bool result = ImGui::DragFloat3(label, values, speed, low, high, format, flags);
  if (auto* ui = SemanticUi::active()) {
    Json copied = unavailableValue("Vector draft has a nonfinite component");
    if (std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]))
      copied = draft(Json{{"x", values[0]}, {"y", values[1]}, {"z", values[2]}}, "vector3");
    const std::string parent = submit(item(label, "vector", parent_id, {"assert"}, copied), parent_id, semanticKey(label));
    Json components = Json::array();
    for (int index = 0; index != 3; ++index) {
      const auto component = ImHashData(&index, sizeof(index), parent_id);
      const std::string key = semanticKey(label) + "." + "xyz"[index];
      auto value = item((labelText(label) + " " + "XYZ"[index]).c_str(), "number", component,
                        {"focus", "edit", "commit", "key", "drag", "assert"}, numeric(values[index]));
      value["drag"] = {{"axis", "horizontal"}, {"distance_unit", "widget_width"}};
      numericInput(value, component); value["capacity_bytes"] = 63;
      value["chords"] = {"Enter", "Escape", "Tab", "Shift+Tab", "Left", "Right", "Home", "End"};
      commit(value, "deactivate", {"enter", "tab"});
      if (low < high) value["range"] = {{"min", low}, {"max", high}};
      const auto ref = submit(std::move(value), component, key);
      if (!ref.empty()) components.push_back({{"key", std::string(1, "xyz"[index])}, {"ref", ref}});
    }
    ui->components(parent, components);
  }
  return result;
}
bool ColorEdit3(const char* label, float values[3], ImGuiColorEditFlags flags) {
  const auto parent_id = ImGui::GetID(label);
  const bool result = ImGui::ColorEdit3(label, values, flags);
  if (auto* ui = SemanticUi::active()) {
    const auto options = ImGui::GetIO().ConfigColorEditFlags;
    const auto display = (flags & ImGuiColorEditFlags_DisplayMask_) ?
        flags & ImGuiColorEditFlags_DisplayMask_ : options & ImGuiColorEditFlags_DisplayMask_;
    const auto encoding = (flags & ImGuiColorEditFlags_InputMask_) ?
        flags & ImGuiColorEditFlags_InputMask_ : options & ImGuiColorEditFlags_InputMask_;
    if ((flags & ImGuiColorEditFlags_NoInputs) || display != ImGuiColorEditFlags_DisplayRGB ||
        encoding != ImGuiColorEditFlags_InputRGB) {
      auto unsupported = item(label, "color", parent_id, {"assert"},
          unavailableValue("This color encoding has no semantic component adapter"));
      unsupported["unsupported_reason"] = "Only RGB component inputs are supported; HSV, hex and hidden inputs require an adapter";
      submit(std::move(unsupported), parent_id, semanticKey(label));
      return result;
    }
    const bool floating = ((flags & ImGuiColorEditFlags_DataTypeMask_) ? flags : ImGui::GetIO().ConfigColorEditFlags) & ImGuiColorEditFlags_Float;
    Json copied = unavailableValue("Color draft has a nonfinite component");
    if (std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2])) {
      copied = draft(Json{{"r", values[0]}, {"g", values[1]}, {"b", values[2]}}, "color");
      copied["encoding"] = "RGB floats 0..1";
      copied["rounding"] = "Widget components follow ImGui display precision; inspect applied binding separately";
    }
    const auto parent = submit(item(label, "color", parent_id, {"assert"}, copied), parent_id, semanticKey(label));
    Json components = Json::array();
    for (int index = 0; index != 3; ++index) {
      const std::string internal = std::string("##") + "XYZ"[index];
      const auto component = ImHashStr(internal.c_str(), 0, parent_id);
      Json number = numeric(values[index]);
      if (!floating && std::isfinite(values[index])) number = draft(static_cast<int>(std::clamp(values[index], 0.0F, 1.0F) * 255 + .5F), "integer");
      auto value = item((labelText(label) + " " + "RGB"[index]).c_str(), "number", component,
                        {"focus", "edit", "commit", "key", "drag", "assert"}, number);
      value["drag"] = {{"axis", "horizontal"}, {"distance_unit", "widget_width"}};
      numericInput(value, component); value["capacity_bytes"] = 63;
      value["chords"] = {"Enter", "Escape", "Tab", "Shift+Tab", "Left", "Right", "Home", "End"};
      value["range"] = {{"min", 0}, {"max", floating ? 1 : 255}};
      commit(value, "deactivate", {"enter", "tab"});
      const std::string key = semanticKey(label) + "." + "rgb"[index];
      const auto ref = submit(std::move(value), component, key);
      if (!ref.empty()) components.push_back({{"key", std::string(1, "rgb"[index])}, {"ref", ref}});
    }
    ui->components(parent, components);
  }
  return result;
}
void ViewportUnsupported() {
  if (!SemanticUi::active()) return;
  auto value = item("Scene viewport", "viewport", 0, Json::array());
  value["unsupported_reason"] = "Custom viewport gestures are outside the semantic widget contract";
  value["state"]["visible"] = unavailableValue("Viewport extent is renderer owned", "unknown");
  value["provenance"]["value"] = "unavailable";
  submit(std::move(value), 0, "scene-viewport");
}
void TextUnformatted(const char* text, const char* end) {
  ImGui::TextUnformatted(text, end);
  textRecord(end ? std::string_view(text, static_cast<std::size_t>(end - text)) : std::string_view(text), "text");
}
void Text(const char* format, ...) {
  va_list arguments; va_start(arguments, format);
  const auto text = formatText(format, arguments);
  ImGui::TextV(format, arguments); va_end(arguments);
  textRecord(text, format);
}
void TextWrapped(const char* format, ...) {
  va_list arguments; va_start(arguments, format);
  const auto text = formatText(format, arguments);
  ImGui::TextWrappedV(format, arguments); va_end(arguments);
  textRecord(text, format);
}
void TextColored(const ImVec4& color, const char* format, ...) {
  va_list arguments; va_start(arguments, format);
  const auto text = formatText(format, arguments);
  ImGui::TextColoredV(color, format, arguments); va_end(arguments);
  textRecord(text, format);
}
}  // namespace editor_automation::widgets
