#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <imgui.h>

#if defined(IMGUI_ENABLE_TEST_ENGINE)
#include "editor/automation/semantic_ui.hpp"
#include "editor/automation/widgets.hpp"
namespace EditorWidgets = editor_automation::widgets;
namespace EditorWidgetMetadata {
inline void viewport() { editor_automation::widgets::ViewportUnsupported(); }
inline void structureChanged(std::string_view family) {
  if (auto* ui = editor_automation::SemanticUi::active()) ui->structureChanged(family);
}
inline void next(std::string_view key, std::string_view field = {},
                 std::string_view unit = {},
                 std::string_view projection = "object", bool immediate = false) {
  if (auto* ui = editor_automation::SemanticUi::active())
    ui->next(key, field, unit, projection, immediate);
}
class Owner {
 public:
  explicit Owner(std::uint64_t id) : ui_(editor_automation::SemanticUi::active()) {
    if (ui_) ui_->pushOwner(id);
  }
  ~Owner() { if (ui_) ui_->popOwner(); }
 private:
  editor_automation::SemanticUi* ui_;
};
class Row {
 public:
  Row(std::string_view family, std::size_t index)
      : ui_(editor_automation::SemanticUi::active()) {
    if (ui_) ui_->pushRow(family, index);
  }
  ~Row() { if (ui_) ui_->popRow(); }
 private:
  editor_automation::SemanticUi* ui_;
};
}  // namespace EditorWidgetMetadata
#else
namespace EditorWidgets = ImGui;
namespace EditorWidgetMetadata {
inline void viewport() {}
inline void structureChanged(std::string_view) {}
inline void next(std::string_view, std::string_view = {},
                 std::string_view = {}, std::string_view = "object",
                 bool = false) {}
class Owner { public: explicit Owner(std::uint64_t) {} };
class Row { public: Row(std::string_view, std::size_t) {} };
}  // namespace EditorWidgetMetadata
#endif
