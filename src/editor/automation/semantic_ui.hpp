#pragma once

#include <imgui.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "editor/automation/application_snapshot.hpp"

namespace editor_automation {
struct EngineItemGeometry {
  ImVec2 min, max, clipped_min, clipped_max;
  int frame{};
};
struct SemanticItem {
  Json record;
  ImGuiID id{};
  std::string window;
  // Current-frame primitive copies, never serialized as a public address.
  std::optional<EngineItemGeometry> engine_geometry{};
};
struct SemanticSnapshot {
  std::vector<SemanticItem> items;
  Json coverage;
  Json active_scope = nullptr, modal_scope = nullptr;
  ObjectRefs object_refs;
  std::map<std::string, std::string> scope_parents, scope_items;
  std::string selection_owner;
};

class SemanticUi {
 public:
  explicit SemanticUi(std::string session_id);
  ~SemanticUi();
  SemanticUi(const SemanticUi&) = delete;
  SemanticUi& operator=(const SemanticUi&) = delete;
  void beginFrame(const EditorDocument& document);
  [[nodiscard]] SemanticSnapshot finishFrame(const EditorDocument& document);
  [[nodiscard]] const SemanticItem& resolve(
      const Json& target, const SemanticSnapshot& snapshot) const;

  // UI-local registrations: values only; no callable or editor pointer survives
  // a widget call. These methods run in the frame/coroutine's exclusive phase.
  static SemanticUi* active() noexcept;
  void pushOwner(EditorObjectId owner);
  void popOwner();
  void pushRow(std::string_view family, std::size_t index);
  void popRow();
  void structureChanged(std::string_view family);
  // Immediate describes the UI handler's application policy. It does not
  // remove explicit Enter/Tab gestures supported by the input widget.
  void next(std::string_view key, std::string_view field = {},
            std::string_view unit = {}, std::string_view projection = "object",
            bool immediate = false);
  [[nodiscard]] std::string scope() const;
  void pushScope(std::string_view key, std::string_view window, bool root = false);
  void popScope();
  void scopeItem(std::string_view ref);
  void limitation(std::string_view reason);
  [[nodiscard]] std::string record(Json record, ImGuiID id,
                                    std::string_view window,
                                    std::string_view default_key);
  void components(std::string_view parent, const Json& components);
  [[nodiscard]] bool hasOwner() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string semanticKey(std::string_view label);
}  // namespace editor_automation
