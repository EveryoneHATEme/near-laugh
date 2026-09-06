#include "editor/editor_ui.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <type_traits>

#include "core/world/door.hpp"
#include "core/world/prototype_level.hpp"
#include "core/world/scene_assets.hpp"
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

std::string pathText(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}
std::filesystem::path pathFromText(const char* text) {
  return std::filesystem::path(std::u8string(text, text + std::strlen(text)));
}

}  // namespace

void EditorUi::draw(EditorDocument& document, bool child_active,
                    std::string_view process_status) {
  if (document_generation_ != document.generation()) {
    document_generation_ = document.generation();
    placing_ = sculpting_ = false;
    placement_hit_.reset();
    placement_object_ = editor_no_object;
    playtest_.cancel();
  }
  ImGui::DockSpaceOverViewport(0, nullptr,
                               ImGuiDockNodeFlags_PassthruCentralNode);
  drawMenu(document);
  drawDocumentSummary(document);
  drawObjects(document);
  drawProperties(document);
  drawPlay(document, child_active, process_status);
  drawValidation(document);
  drawPathModals(document);
  drawPendingModal(document);
}

void EditorUi::finishFrame() { ImGui::Render(); }

void EditorUi::drawPlay(EditorDocument& document, bool child_active,
                        std::string_view process_status) {
  ImGui::SetNextWindowPos({340, 35}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({380, 160}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Playtest");
  if (document.document()) {
    if (ImGui::BeginCombo("Start entry", document.launchEntry().c_str())) {
      for (const auto& entry : document.document()->entries) {
        if (ImGui::Selectable(entry.id.c_str(),
                              entry.id == document.launchEntry()))
          static_cast<void>(document.selectLaunchEntry(entry.id));
      }
      ImGui::EndCombo();
    }
    ImGui::BeginDisabled(
        child_active || playtest_.state() != EditorPlayState::Idle ||
        document.pendingAction().kind != EditorPendingActionKind::None);
    if (ImGui::Button("Play")) {
      const bool draft_changed =
          property_edit_.value() &&
          property_edit_.value() != document.object(document.selection());
      if (!draft_changed || property_edit_.commit(document))
        static_cast<void>(playtest_.request(document, child_active));
    }
    ImGui::EndDisabled();
  } else {
    ImGui::TextUnformatted("Create or open a level to playtest.");
  }
  if (!playtest_.error().empty())
    ImGui::TextWrapped("%s", playtest_.error().c_str());
  if (!process_status.empty())
    ImGui::TextWrapped("%.*s", static_cast<int>(process_status.size()),
                       process_status.data());
  if (playtest_.state() == EditorPlayState::ConfirmSave)
    ImGui::OpenPopup("Save and Play");
  if (ImGui::BeginPopupModal("Save and Play", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(
        "Save the current level before launching the chosen entry.");
    if (ImGui::Button("Save and Play")) {
      static_cast<void>(playtest_.saveAndPlay(document));
      if (playtest_.state() == EditorPlayState::SaveAs)
        openPathModal(true, document);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      playtest_.cancel();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::End();
}

void EditorUi::drawMenu(EditorDocument& document) {
  if (!ImGui::BeginMainMenuBar()) {
    return;
  }
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New Interior")) document.requestNewInterior();
    if (ImGui::MenuItem("Open...")) {
      openPathModal(false, document);
    }
    if (ImGui::MenuItem("Save", "Ctrl+S", false, document.valid())) {
      if (document.path())
        static_cast<void>(document.save());
      else
        openPathModal(true, document);
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
                               ? pathText(*editor_document.path())
                               : std::string("<unsaved>");
  ImGui::TextWrapped("Path: %s", path.c_str());
  ImGui::Text("Format version: %u", document.version);
  if (editor_document.sourceVersion() < level_format_version)
    ImGui::TextWrapped(
        "Opened version %u without changing the file. Explicit Save writes "
        "version %u.",
        editor_document.sourceVersion(), level_format_version);
  ImGui::Text("State: %s", editor_document.dirty() ? "dirty" : "clean");
  if (document.terrain)
    ImGui::Text("Terrain: %zu x %zu samples", prototype_terrain_sample_count,
                prototype_terrain_sample_count);
  else
    ImGui::TextUnformatted("Terrain: absent (interior)");
  ImGui::Text("Solids: %zu / %zu", document.solids.size(),
              level_maximum_solid_count);
  ImGui::Text("Point lights: %zu",
              document.environment_light.point_lights.size());
  ImGui::Text("Entries: %zu / 16; default: %s", document.entries.size(),
              document.default_entry.c_str());
  ImGui::Text("Props: %zu / %zu", document.props.size(), level_maximum_prop_count);
  ImGui::Text("Doors: %zu / %zu", document.doors.size(), level_maximum_door_count);
  ImGui::Text("Light switch: %u / 1", document.light_switch ? 1U : 0U);
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
  if (document.terrain &&
      ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
    showPosition("Origin", document.terrain->origin);
    ImGui::Text("Sample spacing: %.3f", document.terrain->sample_spacing);
    const auto [minimum, maximum] = std::minmax_element(
        document.terrain->heights.begin(), document.terrain->heights.end());
    ImGui::Text("Height range: %.3f to %.3f", *minimum, *maximum);
    if (ImGui::BeginCombo("Terrain material",
                          document.terrain->material.c_str())) {
      for (const auto& material : structuralMaterials())
        if (ImGui::Selectable(material.label.data(),
                              document.terrain->material == material.id))
          static_cast<void>(
              editor_document.setTerrainMaterial(std::string(material.id)));
      ImGui::EndCombo();
    }
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
            commit = true;
          }
          if (ImGui::BeginCombo("Material", value.material.c_str())) {
            for (const auto& material : structuralMaterials())
              if (ImGui::Selectable(material.label.data(),
                                    value.material == material.id)) {
                value.material = material.id;
                commit = true;
              }
            ImGui::EndCombo();
          }
        } else if constexpr (std::is_same_v<T, LevelEntry>) {
          std::array<char, 65> name{};
          std::memcpy(name.data(), value.id.data(),
                      std::min(value.id.size(), name.size() - 1));
          if (ImGui::InputText("Entry ID", name.data(), name.size()))
            value.id = name.data();
          commit |= ImGui::IsItemDeactivatedAfterEdit();
          triple("Foot position", value.pose.foot_position);
          scalar("Yaw (degrees)", value.pose.yaw_degrees, 0.5F);
          if (ImGui::Button("Make default")) {
            static_cast<void>(property_edit_.commit(editor_document));
            static_cast<void>(editor_document.makeSelectedEntryDefault());
          }
        } else if constexpr (std::is_same_v<T, PrototypePointLight>) {
          triple("Position", value.position);
          ImGui::DragFloat3("Light color", value.color.data(), 0.01F, 0, 0,
                            "%.3f");
          commit |= ImGui::IsItemDeactivatedAfterEdit();
          scalar("Intensity", value.intensity);
          scalar("Radius", value.radius);
        } else if constexpr (std::is_same_v<T, PrototypeLightSwitch>) {
          triple("Position", value.position);
          scalar("Yaw (degrees)", value.yaw_degrees, 0.5F);
          int slot = value.point_light_index < prototype_point_light_count
                         ? static_cast<int>(value.point_light_index)
                         : -1;
          if (ImGui::Combo("Linked light", &slot,
                           "Point light 1\0Point light 2\0")) {
            value.point_light_index = static_cast<std::uint32_t>(slot);
            commit = true;
          }
          commit |= ImGui::Checkbox("Initially on", &value.initially_on);
        } else if constexpr (std::is_same_v<T, DoorDefinition>) {
          std::array<char, 65> name{};
          std::memcpy(name.data(), value.id.data(),
                      std::min(value.id.size(), name.size() - 1));
          if (ImGui::InputText("Door ID", name.data(), name.size()))
            value.id = name.data();
          commit |= ImGui::IsItemDeactivatedAfterEdit();
          triple("Bottom hinge", value.hinge_position);
          scalar("Closed yaw", value.closed_yaw_degrees, .5F);
          scalar("Width", value.width);
          scalar("Height", value.height);
          scalar("Thickness", value.thickness, .01F);
          scalar("Opening angle", value.open_angle_degrees, .5F);
          scalar("Angular speed", value.speed_degrees_per_second, .5F);
          int side = static_cast<int>(value.lock_side);
          if (ImGui::Combo("Lock side", &side,
                           "None\0Positive Z\0Negative Z\0")) {
            value.lock_side = static_cast<DoorLockSide>(side);
            commit = true;
          }
          commit |= ImGui::Checkbox("Initially open", &value.initially_open);
          commit |=
              ImGui::Checkbox("Initially locked", &value.initially_locked);
        } else {
          std::array<char, 65> name{};
          std::memcpy(name.data(), value.id.data(),
                      std::min(value.id.size(), name.size() - 1));
          if (ImGui::InputText("Prop ID", name.data(), name.size()))
            value.id = name.data();
          commit |= ImGui::IsItemDeactivatedAfterEdit();
          if (ImGui::BeginCombo("Model", value.model.c_str())) {
            for (const auto& model : sceneModels())
              if (ImGui::Selectable(model.label.data(),
                                    value.model == model.id)) {
                value.model = model.id;
                commit = true;
              }
            ImGui::EndCombo();
          }
          triple("Translation", value.translation);
          scalar("Yaw (degrees)", value.yaw_degrees, 0.5F);
          scalar("Uniform scale", value.uniform_scale, 0.01F);
          if (ImGui::Button("Reset model collision boxes")) {
            if (const auto* model = findSceneModel(value.model)) {
              value.collision_boxes.assign(model->default_boxes.begin(),
                                           model->default_boxes.end());
              commit = true;
            }
          }
          ImGui::BeginDisabled(value.collision_boxes.size() >= 8);
          if (ImGui::Button("Add collision box")) {
            value.collision_boxes.push_back({{0, .5F, 0}, {.5F, .5F, .5F}});
            commit = true;
          }
          ImGui::EndDisabled();
          for (std::size_t i = 0; i < value.collision_boxes.size();) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("Box %zu", i + 1);
            triple("Proxy center", value.collision_boxes[i].center);
            triple("Proxy half extent", value.collision_boxes[i].half_extent);
            const bool remove = ImGui::Button("Remove box");
            ImGui::PopID();
            if (remove) {
              value.collision_boxes.erase(value.collision_boxes.begin() +
                                          static_cast<std::ptrdiff_t>(i));
              commit = true;
            } else
              ++i;
          }
          if (value.collision_boxes.empty())
            ImGui::TextUnformatted("Decorative: no collision.");
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
      ImGui::TextWrapped("Path: %s", pathText(diagnostic.source_path).c_str());
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
      document.requestOpen(pathFromText(path_buffer_.data()));
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      if (save_pending_action_) {
        save_pending_action_ = false;
        static_cast<void>(
            document.resolvePending(EditorPendingDecision::Cancel));
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (ImGui::BeginPopupModal("Save Level As", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Path", path_buffer_.data(), path_buffer_.size());
    ImGui::BeginDisabled(!document.valid());
    if (ImGui::Button("Save")) {
      const bool play_save = playtest_.state() == EditorPlayState::SaveAs;
      const bool saved =
          play_save ? playtest_.saveAsAndPlay(document,
                                              pathFromText(path_buffer_.data()))
                    : document.saveAs(pathFromText(path_buffer_.data()));
      if (saved) {
        if (save_pending_action_) {
          save_pending_action_ = false;
          static_cast<void>(
              document.resolvePending(EditorPendingDecision::Discard));
        }
        ImGui::CloseCurrentPopup();
      } else if (play_save) {
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      playtest_.cancel();
      if (save_pending_action_) {
        save_pending_action_ = false;
        static_cast<void>(
            document.resolvePending(EditorPendingDecision::Cancel));
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void EditorUi::drawPendingModal(EditorDocument& document) {
  if (document.pendingAction().kind != EditorPendingActionKind::None) {
    if (save_pending_action_) return;
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
    if (!document.path()) {
      save_pending_action_ = true;
      openPathModal(true, document);
      ImGui::CloseCurrentPopup();
    } else if (document.resolvePending(EditorPendingDecision::Save)) {
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
    const std::string path = pathText(*document.path());
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
    WorldPosition center{0, 0.5F, 0};
    if (terrain) {
      center = {terrain->origin.x + 24.0F, 0, terrain->origin.z + 24.0F};
      center.y = prototypeTerrainHeightAt(*terrain, center.x, center.z) + 0.5F;
    }
    static_cast<void>(
        document.addSolid({center, {0.5F, 0.5F, 0.5F}, {180, 180, 180, 255}}));
  }
  ImGui::EndDisabled();
  auto selected_value = document.object(document.selection());
  const bool selected_solid =
      selected_value && std::holds_alternative<PrototypeSolid>(*selected_value);
  const bool selected_entry =
      selected_value && std::holds_alternative<LevelEntry>(*selected_value);
  ImGui::SameLine();
  const bool selected_content =
      selected_value &&
      (std::holds_alternative<DoorDefinition>(*selected_value) ||
       std::holds_alternative<PrototypeStaticProp>(*selected_value));
  ImGui::BeginDisabled(!(selected_solid || selected_entry || selected_content));
  if (ImGui::Button("Duplicate"))
    static_cast<void>(document.duplicateSelected());
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!selected_solid && !selected_entry &&
                       !selected_content &&
                       document.selection() != editor_light_switch);
  if (ImGui::Button("Delete")) static_cast<void>(document.removeSelected());
  ImGui::EndDisabled();
  ImGui::BeginDisabled(document.document()->light_switch.has_value());
  if (ImGui::Button("Add light switch")) {
    if (document.addLightSwitch()) sculpting_ = false;
  }
  ImGui::EndDisabled();
  ImGui::BeginDisabled(document.doorIds().size() >= 32);
  if (ImGui::Button("Add door")) static_cast<void>(document.addDoor());
  ImGui::EndDisabled();
  ImGui::BeginDisabled(document.propIds().size() >= 128);
  if (ImGui::BeginCombo("Add prop", "Choose model")) {
    for (const auto& model : sceneModels())
      if (ImGui::Selectable(model.label.data()))
        static_cast<void>(document.addProp(model.id));
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();
  selected_value = document.object(document.selection());
  if (!document.object(document.selection())) placing_ = false;
  ImGui::BeginDisabled(document.selection() == editor_no_object);
  if (ImGui::Checkbox("Place on surface", &placing_) && placing_) {
    static_cast<void>(document.finishTerrainStroke());
    sculpting_ = false;
  }
  ImGui::EndDisabled();
  if (placement_object_ != document.selection()) {
    placement_object_ = document.selection();
    placement_hit_.reset();
    placement_offsets_ = {
        selected_value &&
                std::holds_alternative<PrototypeLightSwitch>(*selected_value)
            ? 1.4F
            : 2.0F,
        0.1F};
    if (selected_value && document.document()->terrain) {
      std::visit(
          [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, PrototypePointLight> ||
                          std::is_same_v<T, PrototypeLightSwitch>) {
              const float h =
                  prototypeTerrainHeightAt(*document.document()->terrain,
                                           value.position.x, value.position.z);
              if (std::isfinite(h))
                placement_offsets_.height = value.position.y - h;
            }
          },
          *selected_value);
    }
  }
  if (placing_) {
    int mode = static_cast<int>(placement_mode_);
    if (ImGui::Combo("Placement surfaces", &mode,
                     "Scene surfaces\0Terrain only\0")) {
      placement_mode_ = static_cast<EditorPlacementMode>(mode);
      placement_hit_.reset();
    }
    if (placement_mode_ == EditorPlacementMode::TerrainOnly &&
        !document.document()->terrain)
      ImGui::TextUnformatted(
          "Terrain-only placement is unavailable in this interior.");
    if (selected_value &&
        (std::holds_alternative<PrototypePointLight>(*selected_value) ||
         std::holds_alternative<PrototypeLightSwitch>(*selected_value))) {
      ImGui::InputFloat("Height above floor (m)", &placement_offsets_.height);
      if (std::holds_alternative<PrototypePointLight>(*selected_value))
        ImGui::InputFloat("Wall offset (m)", &placement_offsets_.outward);
    }
    if (selected_value &&
        std::holds_alternative<DoorDefinition>(*selected_value))
      ImGui::InputFloat("Door floor clearance (m)",
                        &placement_offsets_.door_clearance);
    if (placement_hit_) {
      const char* faces[] = {"Terrain", "Top",     "Underside", "-X wall",
                             "+X wall", "-Z wall", "+Z wall"};
      ImGui::Text("Target %llu: %s, Y %.3f",
                  static_cast<unsigned long long>(placement_hit_->target),
                  faces[static_cast<int>(placement_hit_->face)],
                  placement_hit_->position.y);
      showPosition("Normal", placement_hit_->normal);
      if (selected_value &&
          !editorPlacedObject(*selected_value, *placement_hit_,
                              placement_offsets_))
        ImGui::TextUnformatted("This face cannot place the selected object.");
    }
  }
  ImGui::TextWrapped(
      placing_ ? "Click a surface to place the selected object. Escape "
                 "cancels placement."
               : "Click an object here or in the scene to select. "
                 "Right mouse starts camera navigation.");
  ImGui::Separator();
  const auto entry = [&](EditorObjectId id, const std::string& label) {
    if (ImGui::Selectable(label.c_str(), document.selection() == id))
      document.select(id);
  };
  if (ImGui::Button("Add entry")) {
    const auto& level = *document.document();
    static_cast<void>(document.addEntry(level.entries.empty()
                                            ? PrototypePlayerSpawn{}
                                            : level.entries.front().pose));
  }
  for (std::size_t i = 0; i < document.entryIds().size(); ++i)
    entry(document.entryIds()[i],
          "Entry: " + document.document()->entries[i].id);
  entry(editor_first_light, "Point light 1");
  entry(editor_first_light + 1, "Point light 2");
  for (std::size_t i = 0; i < document.propIds().size(); ++i)
    entry(document.propIds()[i], "Prop: " + document.document()->props[i].id);
  for (std::size_t i = 0; i < document.doorIds().size(); ++i)
    entry(document.doorIds()[i], "Door: " + document.document()->doors[i].id);
  if (document.document()->light_switch)
    entry(editor_light_switch, "Light switch");
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
        document.valid()) {
      if (document.path())
        static_cast<void>(document.save());
      else
        openPathModal(true, document);
    }
  }
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const auto ray = editorPointerRay(camera, io.MousePos.x - viewport->Pos.x,
                                    io.MousePos.y - viewport->Pos.y,
                                    viewport->Size.x, viewport->Size.y);
  if (sculpting_ && (!document.document() || !document.document()->terrain))
    sculpting_ = false;
  if (sculpting_) {
    return updateEditorTerrainViewport(
        document, ray,
        io.WantCaptureMouse ||
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || popup,
        navigating, ImGui::IsMouseClicked(ImGuiMouseButton_Left),
        ImGui::IsMouseDown(ImGuiMouseButton_Left),
        io.MouseDelta.x != 0 || io.MouseDelta.y != 0);
  }
  if (placing_) {
    placement_hit_ = updateEditorPlacementViewport(
        document, ray,
        io.WantCaptureMouse ||
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || popup,
        navigating, ImGui::IsMouseClicked(ImGuiMouseButton_Left),
        placement_mode_, placement_offsets_);
    return placement_hit_ ? std::optional{placement_hit_->position}
                          : std::nullopt;
  }
  placement_hit_.reset();
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
