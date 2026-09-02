#include "editor/editor_ui.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>

#include "editor/editor_document.hpp"

namespace {
const char* diagnosticCategoryName(LevelDiagnosticCategory category) {
  switch (category) {
    case LevelDiagnosticCategory::Parse:
      return "parse";
    case LevelDiagnosticCategory::Validation:
      return "validation";
    case LevelDiagnosticCategory::Filesystem:
      return "filesystem";
  }
  return "unknown";
}

void showPosition(const char* label, const WorldPosition& position) {
  ImGui::Text("%s: (%.3f, %.3f, %.3f)", label, position.x, position.y,
              position.z);
}

void showExtent(const char* label, const WorldExtent& extent) {
  ImGui::Text("%s: (%.3f, %.3f, %.3f)", label, extent.x, extent.y, extent.z);
}
}  // namespace

void EditorUi::draw(EditorDocument& document) {
  ImGui::DockSpaceOverViewport(0, nullptr,
                               ImGuiDockNodeFlags_PassthruCentralNode);
  drawMenu(document);
  drawDocumentSummary(document);
  drawProperties(document);
  drawValidation(document);
  drawPathModals(document);
  drawPendingModal(document);
}

void EditorUi::finishFrame() { ImGui::Render(); }

void EditorUi::drawMenu(EditorDocument& document) {
  if (!ImGui::BeginMainMenuBar()) {
    return;
  }
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open...")) {
      openPathModal(false, document);
    }
    if (ImGui::MenuItem("Save", nullptr, false,
                        document.document().has_value())) {
      static_cast<void>(document.save());
    }
    if (ImGui::MenuItem("Save As...", nullptr, false,
                        document.document().has_value())) {
      openPathModal(true, document);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Close", nullptr, false,
                        document.document().has_value())) {
      document.requestClose();
    }
    if (ImGui::MenuItem("Exit")) {
      document.requestExit();
    }
    ImGui::EndMenu();
  }
  ImGui::EndMainMenuBar();
}

void EditorUi::drawDocumentSummary(const EditorDocument& editor_document) {
  ImGui::Begin("Document Summary");
  if (!editor_document.document()) {
    ImGui::TextUnformatted("No level is open.");
    ImGui::End();
    return;
  }
  const LevelDocument& document = *editor_document.document();
  const std::string path = editor_document.path()
                               ? editor_document.path()->string()
                               : std::string("<unsaved>");
  ImGui::TextWrapped("Path: %s", path.c_str());
  ImGui::Text("Format version: %u", document.version);
  ImGui::Text("State: %s", editor_document.dirty() ? "dirty" : "clean");
  ImGui::Text("Terrain: %zu x %zu samples", prototype_terrain_sample_count,
              prototype_terrain_sample_count);
  ImGui::Text("Solids: %zu / %zu", document.solids.size(),
              level_maximum_solid_count);
  ImGui::Text("Point lights: %zu",
              document.environment_light.point_lights.size());
  ImGui::TextUnformatted("Player spawn: 1");
  ImGui::TextUnformatted("Static prop: 1 packaged chair");
  ImGui::End();
}

void EditorUi::drawProperties(const EditorDocument& editor_document) {
  ImGui::Begin("Read-only Properties");
  if (!editor_document.document()) {
    ImGui::TextUnformatted("Open a level to inspect its bounded contents.");
    ImGui::End();
    return;
  }
  const LevelDocument& document = *editor_document.document();
  if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
    showPosition("Origin", document.terrain.origin);
    ImGui::Text("Sample spacing: %.3f", document.terrain.sample_spacing);
    const auto [minimum, maximum] = std::minmax_element(
        document.terrain.heights.begin(), document.terrain.heights.end());
    ImGui::Text("Height range: %.3f to %.3f", *minimum, *maximum);
  }
  if (ImGui::CollapsingHeader("Solids")) {
    for (std::size_t index = 0; index < document.solids.size(); ++index) {
      ImGui::PushID(static_cast<int>(index));
      if (ImGui::TreeNode("Solid", "Solid %zu", index)) {
        showPosition("Center", document.solids[index].center);
        showExtent("Half extent", document.solids[index].half_extent);
        ImGui::Text("Kind: %d", static_cast<int>(document.solids[index].kind));
        ImGui::Text("Surface: %u",
                    static_cast<unsigned>(document.solids[index].surface));
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
  }
  if (ImGui::CollapsingHeader("Lights")) {
    ImGui::Text("Ambient intensity: %.3f",
                document.environment_light.ambient_intensity);
    for (std::size_t index = 0;
         index < document.environment_light.point_lights.size(); ++index) {
      const PrototypePointLight& light =
          document.environment_light.point_lights[index];
      ImGui::SeparatorText(("Point light " + std::to_string(index)).c_str());
      showPosition("Position", light.position);
      ImGui::Text("Color: (%.3f, %.3f, %.3f)", light.color[0], light.color[1],
                  light.color[2]);
      ImGui::Text("Intensity: %.3f, radius: %.3f", light.intensity,
                  light.radius);
    }
  }
  if (ImGui::CollapsingHeader("Player Spawn")) {
    showPosition("Foot position", document.player_spawn.foot_position);
    ImGui::Text("Yaw: %.3f degrees", document.player_spawn.yaw_degrees);
  }
  if (ImGui::CollapsingHeader("Static Prop")) {
    showPosition("Translation", document.static_prop.translation);
    ImGui::Text("Yaw: %.3f degrees", document.static_prop.yaw_degrees);
    ImGui::Text("Scale: %.3f", document.static_prop.uniform_scale);
    showPosition("Proxy center", document.static_prop.box_proxy_center);
    showExtent("Proxy half extent", document.static_prop.box_proxy_half_extent);
  }
  ImGui::End();
}

void EditorUi::drawValidation(const EditorDocument& document) {
  ImGui::Begin("Validation");
  if (document.document() && document.diagnostics().empty()) {
    ImGui::TextColored({0.3F, 0.9F, 0.4F, 1.0F}, "Level is valid.");
  } else if (!document.document() && document.diagnostics().empty()) {
    ImGui::TextUnformatted("No level is open.");
  }
  for (const LevelDiagnostic& diagnostic : document.diagnostics()) {
    ImGui::Separator();
    ImGui::Text("%s", diagnosticCategoryName(diagnostic.category));
    if (!diagnostic.source_path.empty()) {
      ImGui::TextWrapped("Path: %s", diagnostic.source_path.string().c_str());
    }
    if (!diagnostic.document_path.empty()) {
      ImGui::TextWrapped("Field/object: %s", diagnostic.document_path.c_str());
    }
    ImGui::TextWrapped("%s", diagnostic.message.c_str());
  }
  ImGui::End();
}

void EditorUi::drawPathModals(EditorDocument& document) {
  if (open_path_popup_) {
    ImGui::OpenPopup("Open Level");
    open_path_popup_ = false;
  }
  if (save_as_popup_) {
    ImGui::OpenPopup("Save Level As");
    save_as_popup_ = false;
  }
  if (ImGui::BeginPopupModal("Open Level", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Path", path_buffer_.data(), path_buffer_.size());
    if (ImGui::Button("Open")) {
      document.requestOpen(std::filesystem::path(path_buffer_.data()));
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (ImGui::BeginPopupModal("Save Level As", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Path", path_buffer_.data(), path_buffer_.size());
    if (ImGui::Button("Save")) {
      if (document.saveAs(std::filesystem::path(path_buffer_.data()))) {
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void EditorUi::drawPendingModal(EditorDocument& document) {
  if (document.pendingAction().kind != EditorPendingActionKind::None) {
    ImGui::OpenPopup("Unsaved Changes");
  }
  if (!ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  ImGui::TextUnformatted(
      "The current level has unsaved changes. Save before continuing?");
  if (ImGui::Button("Save")) {
    if (document.resolvePending(EditorPendingDecision::Save)) {
      ImGui::CloseCurrentPopup();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Discard")) {
    static_cast<void>(document.resolvePending(EditorPendingDecision::Discard));
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    static_cast<void>(document.resolvePending(EditorPendingDecision::Cancel));
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

void EditorUi::openPathModal(bool save_as, const EditorDocument& document) {
  path_buffer_.fill('\0');
  if (save_as && document.path()) {
    const std::string path = document.path()->string();
    const std::size_t length = std::min(path.size(), path_buffer_.size() - 1);
    std::memcpy(path_buffer_.data(), path.data(), length);
  }
  save_as_popup_ = save_as;
  open_path_popup_ = !save_as;
}
