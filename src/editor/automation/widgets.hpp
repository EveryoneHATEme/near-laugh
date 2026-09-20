#pragma once

#include <imgui.h>

namespace editor_automation::widgets {
using namespace ImGui;

bool Begin(const char* name, bool* open = nullptr, ImGuiWindowFlags flags = 0);
void End();
bool BeginChild(const char* id, const ImVec2& size = {},
                ImGuiChildFlags child_flags = 0, ImGuiWindowFlags window_flags = 0);
void EndChild();
bool BeginMainMenuBar();
void EndMainMenuBar();
bool BeginMenu(const char* label, bool enabled = true);
void EndMenu();
bool BeginCombo(const char* label, const char* preview, ImGuiComboFlags flags = 0);
void EndCombo();
bool BeginPopupModal(const char* name, bool* open = nullptr,
                     ImGuiWindowFlags flags = 0);
void EndPopup();
bool Button(const char* label, const ImVec2& size = {});
bool MenuItem(const char* label, const char* shortcut = nullptr,
               bool selected = false, bool enabled = true);
bool Checkbox(const char* label, bool* value);
bool Selectable(const char* label, bool selected = false,
                 ImGuiSelectableFlags flags = 0, const ImVec2& size = {});
bool Combo(const char* label, int* current, const char* separated_items,
            int height = -1);
bool CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0);
bool InputText(const char* label, char* buffer, size_t capacity,
                ImGuiInputTextFlags flags = 0,
                ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
bool InputTextMultiline(const char* label, char* buffer, size_t capacity,
                         const ImVec2& size = {}, ImGuiInputTextFlags flags = 0,
                         ImGuiInputTextCallback callback = nullptr,
                         void* user_data = nullptr);
bool InputFloat(const char* label, float* value, float step = 0,
                 float fast_step = 0, const char* format = "%.3f",
                 ImGuiInputTextFlags flags = 0);
bool DragFloat(const char* label, float* value, float speed = 1,
                float low = 0, float high = 0, const char* format = "%.3f",
                ImGuiSliderFlags flags = 0);
bool DragFloat3(const char* label, float values[3], float speed = 1,
                 float low = 0, float high = 0, const char* format = "%.3f",
                 ImGuiSliderFlags flags = 0);
bool SliderFloat(const char* label, float* value, float low, float high,
                  const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool ColorEdit3(const char* label, float values[3], ImGuiColorEditFlags flags = 0);
void Text(const char* format, ...) IM_FMTARGS(1);
void TextWrapped(const char* format, ...) IM_FMTARGS(1);
void TextColored(const ImVec4& color, const char* format, ...) IM_FMTARGS(2);
void ViewportUnsupported();
void TextUnformatted(const char* text, const char* end = nullptr);
}  // namespace editor_automation::widgets
