#include "editor/editor_ui.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <type_traits>

#include "core/world/prototype_level.hpp"
#include "editor/editor_document.hpp"
#include "editor/editor_picking.hpp"

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

}  // namespace

void EditorUi::draw(EditorDocument& document) {
  ImGui::DockSpaceOverViewport(0, nullptr,
                               ImGuiDockNodeFlags_PassthruCentralNode);
  drawMenu(document);
  drawDocumentSummary(document);
  drawObjects(document);
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
    if (ImGui::MenuItem("Save", "Ctrl+S", false, document.valid())) {
      static_cast<void>(document.save());
    }
    if (ImGui::MenuItem("Save As...", nullptr, false, document.valid())) {
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
  if (ImGui::BeginMenu("Edit")) {
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, document.canUndo()))
      static_cast<void>(document.undo());
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, document.canRedo()))
      static_cast<void>(document.redo());
    ImGui::EndMenu();
  }
  ImGui::EndMainMenuBar();
}

void EditorUi::drawDocumentSummary(const EditorDocument& editor_document) {
  ImGui::SetNextWindowPos({10, 35}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({320, 245}, ImGuiCond_FirstUseEver);
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

void EditorUi::drawProperties(EditorDocument& editor_document) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos({viewport->Size.x - 390, 35}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({380, 500}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Properties");
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
    drawTerrainBrush(editor_document);
  }
  property_edit_.synchronize(editor_document);
  if (!property_edit_.value()) {
    ImGui::TextUnformatted("Select an object in the list or scene to edit it.");
    ImGui::End();
    return;
  }
  bool commit = false;
  const auto scalar = [&](const char* label, float& value,
                          float speed = 0.05F) {
    ImGui::DragFloat(label, &value, speed, 0, 0, "%.3f");
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  const auto triple = [&](const char* label, auto& value) {
    float values[] = {value.x, value.y, value.z};
    if (ImGui::DragFloat3(label, values, 0.05F, 0, 0, "%.3f")) {
      value.x = values[0];
      value.y = values[1];
      value.z = values[2];
    }
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  std::visit(
      [&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, PrototypeSolid>) {
          triple("Center", value.center);
          triple("Half extent", value.half_extent);
          float tint[] = {value.color[0] / 255.0F, value.color[1] / 255.0F,
                          value.color[2] / 255.0F};
          if (ImGui::ColorEdit3("Tint", tint, ImGuiColorEditFlags_NoPicker)) {
            for (int i = 0; i < 3; ++i)
              value.color[i] = static_cast<std::uint8_t>(
                  std::clamp(tint[i], 0.0F, 1.0F) * 255.0F + 0.5F);
          }
          commit |= ImGui::IsItemDeactivatedAfterEdit();
          int kind = static_cast<int>(value.kind);
          if (ImGui::Combo("Kind", &kind,
                           "Floor\0Boundary\0Obstacle\0Walkable step\0Low "
                           "clearance\0")) {
            value.kind = static_cast<PrototypeSolidKind>(kind);
            value.surface = kind == 0   ? PrototypeSurface::Floor
                            : kind == 1 ? PrototypeSurface::Boundary
                                        : PrototypeSurface::Obstacle;
            commit = true;
          }
          int surface = static_cast<int>(value.surface);
          if (ImGui::Combo("Surface", &surface,
                           "Floor\0Boundary\0Obstacle\0")) {
            value.surface = static_cast<PrototypeSurface>(surface);
            commit = true;
          }
          ImGui::TextWrapped(
              "Surface must match the kind before saving: floor, boundary, or "
              "obstacle for other kinds.");
        } else if constexpr (std::is_same_v<T, PrototypePlayerSpawn>) {
          triple("Foot position", value.foot_position);
          scalar("Yaw (degrees)", value.yaw_degrees, 0.5F);
        } else if constexpr (std::is_same_v<T, PrototypePointLight>) {
          triple("Position", value.position);
          ImGui::DragFloat3("Light color", value.color.data(), 0.01F, 0, 0,
                            "%.3f");
          commit |= ImGui::IsItemDeactivatedAfterEdit();
          scalar("Intensity", value.intensity);
          scalar("Radius", value.radius);
        } else {
          triple("Translation", value.translation);
          scalar("Yaw (degrees)", value.yaw_degrees, 0.5F);
          scalar("Uniform scale", value.uniform_scale, 0.01F);
          ImGui::TextUnformatted("Surface: obstacle");
          triple("Proxy center", value.box_proxy_center);
          triple("Proxy half extent", value.box_proxy_half_extent);
        }
      },
      *property_edit_.value());
  if (commit) static_cast<void>(property_edit_.commit(editor_document));
  ImGui::End();
}

void EditorUi::drawValidation(const EditorDocument& document) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos({viewport->Size.x - 390, 545},
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({380, 280}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Validation");
  if (document.terrainStrokeActive()) {
    ImGui::TextUnformatted("Stroke active; validation refreshes on release.");
  } else if (document.document() && document.diagnostics().empty()) {
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
    if (const auto& location = diagnostic.terrain_location) {
      if (location->triangle)
        ImGui::Text("Cell X=%zu Z=%zu, triangle %u", location->x, location->z,
                    *location->triangle + 1);
      else
        ImGui::Text("Sample X=%zu Z=%zu", location->x, location->z);
    }
  }
  if (!document.editError().empty())
    ImGui::TextWrapped("Edit rejected: %s", document.editError().c_str());
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
    ImGui::BeginDisabled(!document.valid());
    if (ImGui::Button("Save")) {
      if (document.saveAs(std::filesystem::path(path_buffer_.data()))) {
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndDisabled();
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
  if (document.pendingAction().kind == EditorPendingActionKind::None) {
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }
  ImGui::TextUnformatted(
      "The current level has unsaved changes. Save before continuing?");
  ImGui::BeginDisabled(!document.valid());
  if (ImGui::Button("Save")) {
    if (document.resolvePending(EditorPendingDecision::Save)) {
      ImGui::CloseCurrentPopup();
    }
  }
  ImGui::EndDisabled();
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

void EditorUi::drawObjects(EditorDocument& document) {
  ImGui::SetNextWindowPos({10, 290}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({320, 480}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Objects");
  if (!document.document()) {
    placing_ = false;
    ImGui::TextUnformatted("Open a level to place objects.");
    ImGui::End();
    return;
  }
  ImGui::BeginDisabled(document.solidIds().size() >= level_maximum_solid_count);
  if (ImGui::Button("Add solid")) {
    const auto& terrain = document.document()->terrain;
    WorldPosition center{terrain.origin.x + 24.0F, 0, terrain.origin.z + 24.0F};
    center.y = prototypeTerrainHeightAt(terrain, center.x, center.z) + 0.5F;
    static_cast<void>(
        document.addSolid({center, {0.5F, 0.5F, 0.5F}, {180, 180, 180, 255}}));
  }
  ImGui::EndDisabled();
  const bool selected_solid = document.selection() >= editor_first_solid;
  ImGui::SameLine();
  ImGui::BeginDisabled(!selected_solid ||
                       document.solidIds().size() >= level_maximum_solid_count);
  if (ImGui::Button("Duplicate"))
    static_cast<void>(document.duplicateSelected());
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!selected_solid);
  if (ImGui::Button("Delete")) static_cast<void>(document.removeSelected());
  ImGui::EndDisabled();
  if (!document.object(document.selection())) placing_ = false;
  ImGui::BeginDisabled(document.selection() == editor_no_object);
  if (ImGui::Checkbox("Place on terrain", &placing_) && placing_) {
    static_cast<void>(document.finishTerrainStroke());
    sculpting_ = false;
  }
  ImGui::EndDisabled();
  ImGui::TextWrapped(placing_
                         ? "Click terrain to place the selected object. Escape "
                           "cancels placement."
                         : "Click an object here or in the scene to select. "
                           "Right mouse starts camera navigation.");
  ImGui::Separator();
  const auto entry = [&](EditorObjectId id, const std::string& label) {
    if (ImGui::Selectable(label.c_str(), document.selection() == id))
      document.select(id);
  };
  entry(editor_spawn, "Player spawn");
  entry(editor_first_light, "Point light 1");
  entry(editor_first_light + 1, "Point light 2");
  entry(editor_prop, "Chair / box proxy");
  for (std::size_t i = 0; i < document.solidIds().size(); ++i) {
    const auto& solid = document.document()->solids[i];
    const char* kinds[] = {"Floor", "Boundary", "Obstacle", "Walkable step",
                           "Low clearance"};
    entry(document.solidIds()[i],
          std::string(kinds[static_cast<int>(solid.kind)]) + " " +
              std::to_string(i + 1));
  }
  ImGui::End();
}

std::optional<WorldPosition> EditorUi::updateViewport(EditorDocument& document,
                                                      const CameraFrame& camera,
                                                      bool navigating) {
  const ImGuiIO& io = ImGui::GetIO();
  const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  if (!navigating && !io.WantTextInput && !ImGui::IsAnyItemActive() && !popup) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      placing_ = false;
      sculpting_ = false;
      static_cast<void>(document.finishTerrainStroke());
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      if (io.KeyShift)
        static_cast<void>(document.redo());
      else
        static_cast<void>(document.undo());
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
      static_cast<void>(document.redo());
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
      static_cast<void>(document.duplicateSelected());
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
      static_cast<void>(document.removeSelected());
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) &&
        document.valid())
      static_cast<void>(document.save());
  }
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const auto ray = editorPointerRay(camera, io.MousePos.x - viewport->Pos.x,
                                    io.MousePos.y - viewport->Pos.y,
                                    viewport->Size.x, viewport->Size.y);
  if (sculpting_) {
    return updateEditorTerrainViewport(
        document, ray,
        io.WantCaptureMouse ||
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || popup,
        navigating, ImGui::IsMouseClicked(ImGuiMouseButton_Left),
        ImGui::IsMouseDown(ImGuiMouseButton_Left),
        io.MouseDelta.x != 0 || io.MouseDelta.y != 0);
  }
  return updateEditorViewport(
      document, ray,
      io.WantCaptureMouse ||
          ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || popup,
      navigating, ImGui::IsMouseClicked(ImGuiMouseButton_Left), placing_);
}

void EditorUi::drawTerrainBrush(EditorDocument& document) {
  if (ImGui::Checkbox("Sculpt terrain", &sculpting_)) {
    static_cast<void>(document.finishTerrainStroke());
    if (sculpting_) placing_ = false;
  }
  if (!sculpting_) return;
  if (!ImGui::IsAnyItemActive()) brush_draft_ = document.terrainBrush();
  bool commit = false;
  int mode = static_cast<int>(brush_draft_.mode);
  if (ImGui::Combo("Brush mode", &mode, "Raise\0Lower\0Smooth\0")) {
    brush_draft_.mode = static_cast<EditorBrushMode>(mode);
    commit = true;
  }
  const auto control = [&](const char* label, float& value, float low,
                           float high) {
    ImGui::DragFloat(label, &value, 0.01F, low, high, "%.3f");
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  control("Brush radius (m)", brush_draft_.radius, 0.5F, 8);
  if (brush_draft_.mode == EditorBrushMode::Smooth)
    control("Smooth strength", brush_draft_.smooth_strength, 0, 1);
  else
    control("Strength (m/stamp)", brush_draft_.strength, 0.01F, 1);
  control("Falloff", brush_draft_.falloff, 0, 1);
  if (commit) {
    static_cast<void>(document.setTerrainBrush(brush_draft_));
    brush_draft_ = document.terrainBrush();
  }
  ImGui::TextWrapped(
      "Left-drag terrain to sculpt. One gesture is one undo. "
      "Radius: 0.5-8 m; raise/lower: 0.01-1 m; smooth/falloff: 0-1. "
      "Escape returns to object selection.");
}
