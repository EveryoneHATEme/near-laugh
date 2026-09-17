#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <type_traits>

#include "editor/editor_ui.hpp"

namespace {
enum class PageAction { None, Previous, Next, Add, Remove };
}

void EditorUi::drawHouseholdObjects(EditorDocument& document) {
  if (!ImGui::CollapsingHeader("Household", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  const std::array buttons{"Add box", "Add document", "Add radio control"};
  const std::array labels{"Box: ", "Document: ", "Radio control: "};
  const std::array limits{level_maximum_household_box_count,
                          level_maximum_household_document_count,
                          level_maximum_household_radio_count};
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    const auto kind = static_cast<EditorHouseholdKind>(i);
    ImGui::BeginDisabled(document.householdIds(kind).size() >= limits[i]);
    const bool add = ImGui::Button(buttons[i]);
    ImGui::EndDisabled();
    if (add && commitSelectionDraft(document) && document.addHousehold(kind))
      placing_ = sculpting_ = false;
  }
  for (std::size_t kind = 0; kind < labels.size(); ++kind)
    for (const auto id :
         document.householdIds(static_cast<EditorHouseholdKind>(kind))) {
      const auto value = *document.object(id);
      std::visit(
          [&](const auto& record) {
            if constexpr (requires { record.id; }) {
              ImGui::PushID(static_cast<int>(id));
              if (ImGui::Selectable((labels[kind] + record.id).c_str(),
                                    document.selection() == id))
                selectObject(document, id);
              ImGui::PopID();
            }
          },
          value);
    }
  ImGui::Separator();
}

void EditorUi::drawHouseholdProperties(EditorDocument& document) {
  if (readable_object_ != document.selection() ||
      readable_generation_ != document.generation()) {
    readable_object_ = document.selection();
    readable_generation_ = document.generation();
    readable_page_ = 0;
    readable_preview_.reset();
    readable_preview_revision_.reset();
    readable_preview_error_.clear();
  }
  const auto& level = *document.document();
  bool commit = false, select_prop = false;
  PageAction page_action = PageAction::None;
  const auto text = [&](const char* label, std::string& value,
                        std::size_t capacity, bool multiline = false) {
    // Bounded UTF-8 bytes permit the full scalar profile, including Cyrillic.
    std::array<char, level_maximum_document_page_scalars * 4 + 1> buffer{};
    std::memcpy(buffer.data(), value.data(),
                std::min(value.size(), capacity - 1));
    const bool edited = multiline
                            ? ImGui::InputTextMultiline(label, buffer.data(),
                                                        capacity, {0, 130})
                            : ImGui::InputText(label, buffer.data(), capacity);
    if (edited) value = buffer.data();
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  const auto position = [&](const char* label, WorldPosition& value) {
    float xyz[]{value.x, value.y, value.z};
    if (ImGui::DragFloat3(label, xyz, .05F, 0, 0, "%.3f"))
      value = {xyz[0], xyz[1], xyz[2]};
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  const auto yaw = [&](float& value) {
    ImGui::DragFloat("Initial yaw (degrees)", &value, .5F, 0, 0, "%.3f");
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  const auto reference = [&](const char* label, std::string& value,
                             const auto& choices) {
    if (ImGui::BeginCombo(label, value.empty() ? "<missing>" : value.c_str())) {
      for (const auto& choice : choices)
        if (ImGui::Selectable(choice.id.c_str(), value == choice.id)) {
          value = choice.id;
          commit = true;
        }
      ImGui::EndCombo();
    }
    if (std::none_of(choices.begin(), choices.end(),
                     [&](const auto& choice) { return choice.id == value; }))
      ImGui::TextWrapped("Unresolved %s: %s", label,
                         value.empty() ? "<empty>" : value.c_str());
  };
  std::visit(
      [&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, HouseholdBoxDefinition>) {
          ImGui::TextUnformatted("Physical box: 0.30 m cube, 1 kg.");
          ImGui::TextWrapped(
              "Initial pose only. Use Play to test falling, pickup, drop and "
              "throw.");
          text("Box ID", value.id, 65);
          position("Initial center", value.center);
          yaw(value.yaw_degrees);
        } else if constexpr (std::is_same_v<T, HouseholdDocumentDefinition>) {
          text("Document ID", value.id, 65);
          position("Panel position", value.position);
          yaw(value.yaw_degrees);
          text("Document title", value.title,
               level_maximum_document_title_scalars * 4 + 1);
          if (!value.pages.empty()) {
            readable_page_ = std::min(readable_page_, value.pages.size() - 1);
            ImGui::Text("Page %zu of %zu", readable_page_ + 1,
                        value.pages.size());
            ImGui::PushID(static_cast<int>(readable_page_));
            text("Page text", value.pages[readable_page_],
                 level_maximum_document_page_scalars * 4 + 1, true);
            ImGui::PopID();
            ImGui::TextWrapped(
                "Enter adds a line; Ctrl+Enter commits the page. Title: 80 "
                "characters; page: 480.");
          } else {
            readable_page_ = 0;
            ImGui::TextUnformatted("No pages. Add a page before Save or Play.");
          }
          ImGui::BeginDisabled(value.pages.empty() || readable_page_ == 0);
          if (ImGui::Button("Previous page"))
            page_action = PageAction::Previous;
          ImGui::EndDisabled();
          ImGui::SameLine();
          ImGui::BeginDisabled(value.pages.empty() ||
                               readable_page_ + 1 >= value.pages.size());
          if (ImGui::Button("Next page")) page_action = PageAction::Next;
          ImGui::EndDisabled();
          ImGui::BeginDisabled(value.pages.size() >=
                               level_maximum_document_page_count);
          if (ImGui::Button("Add page")) page_action = PageAction::Add;
          ImGui::EndDisabled();
          ImGui::SameLine();
          ImGui::BeginDisabled(value.pages.size() <= 1);
          if (ImGui::Button("Remove page")) page_action = PageAction::Remove;
          ImGui::EndDisabled();
        } else if constexpr (std::is_same_v<T, HouseholdRadioDefinition>) {
          ImGui::TextUnformatted(
              "Radio control (separate from its static prop).");
          text("Radio control ID", value.id, 65);
          reference("Radio prop", value.prop, level.props);
          reference("Radio source", value.source, level.audio.sources);
          commit |= ImGui::Checkbox("Initially on", &value.initially_on);
          const auto prop = std::find_if(level.props.begin(), level.props.end(),
                                         [&](const auto& candidate) {
                                           return candidate.id == value.prop;
                                         });
          if (prop != level.props.end() && (prop->model != "apartment-radio" ||
                                            !prop->collision_boxes.empty()))
            ImGui::TextWrapped(
                "Radio prop must use apartment-radio with no collision boxes.");
          ImGui::TextWrapped(
              "Choose an exclusive spatial source with autoplay off and a "
              "captioned ambience loop. Link and ownership errors appear in "
              "Validation.");
          ImGui::BeginDisabled(prop == level.props.end());
          select_prop = ImGui::Button("Select radio prop");
          ImGui::EndDisabled();
        }
      },
      *property_edit_.value());
  const auto error = editorHouseholdFieldError(*property_edit_.value());
  if (!error.empty()) ImGui::TextWrapped("Draft error: %s", error.c_str());
  if (!document.editError().empty())
    ImGui::TextWrapped("Edit rejected: %s", document.editError().c_str());

  if ((commit || page_action != PageAction::None || select_prop) &&
      !commitSelectionDraft(document))
    return;
  // Committing replaces the draft; obtain fresh owned values before any
  // page/list mutation. A text commit and a page addition are separate edits.
  if (page_action != PageAction::None) {
    auto value = std::get<HouseholdDocumentDefinition>(
        *document.object(document.selection()));
    switch (page_action) {
      case PageAction::Previous:
        if (readable_page_ > 0) --readable_page_;
        break;
      case PageAction::Next:
        if (readable_page_ + 1 < value.pages.size()) ++readable_page_;
        break;
      case PageAction::Add:
        value.pages.emplace_back("Текст новой страницы.");
        if (document.replaceObject(document.selection(), value))
          readable_page_ = value.pages.size() - 1;
        break;
      case PageAction::Remove:
        if (value.pages.size() > 1) {
          value.pages.erase(value.pages.begin() +
                            static_cast<std::ptrdiff_t>(readable_page_));
          if (document.replaceObject(document.selection(), value))
            readable_page_ = std::min(readable_page_, value.pages.size() - 1);
        }
        break;
      case PageAction::None:
        break;
    }
    property_edit_.synchronize(document);
  }
  if (select_prop) {
    const auto radio = std::get<HouseholdRadioDefinition>(
        *document.object(document.selection()));
    for (std::size_t i = 0; i < level.props.size(); ++i)
      if (level.props[i].id == radio.prop) {
        selectObject(document, document.propIds()[i]);
        break;
      }
  }
}

void EditorUi::drawReadablePreview(const EditorDocument& document) {
  const auto selected = document.object(document.selection());
  if (!selected ||
      !std::holds_alternative<HouseholdDocumentDefinition>(*selected))
    return;
  const auto& value = std::get<HouseholdDocumentDefinition>(*selected);
  if (!readable_preview_revision_ ||
      *readable_preview_revision_ != document.revision() ||
      readable_preview_page_ != readable_page_) {
    readable_preview_revision_ = document.revision();
    readable_preview_page_ = readable_page_;
    try {
      if (!readable_font_)
        throw std::invalid_argument("Trusted readable font is unavailable.");
      if (value.pages.empty())
        throw std::invalid_argument("Add at least one page.");
      const std::string controls = "Страница " +
                                   std::to_string(readable_page_ + 1) + "/" +
                                   std::to_string(value.pages.size()) +
                                   " | A/D: страницы | E/Escape: закрыть";
      auto preview = readable_font_->layout(
          {}, 800, 600,
          {{value.title, value.pages.at(readable_page_), controls}, {}, {}});
      readable_preview_ = std::move(preview);
      readable_last_good_page_ = readable_page_;
      readable_preview_error_.clear();
    } catch (const std::exception& error) {
      readable_preview_error_ = "Document '" + value.id + "', page " +
                                std::to_string(readable_page_ + 1) + ": " +
                                error.what();
    }
  }
  ImGui::SetNextWindowPos({340, 475}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({780, 315}, ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Readable preview")) {
    ImGui::Text("Minimum framebuffer: 800x600. Page %zu of %zu.",
                readable_page_ + 1, value.pages.size());
    ImGui::TextWrapped(
        "Caption and household hint lanes remain reserved below this panel.");
    if (property_edit_.value() && property_edit_.value() != selected)
      ImGui::TextUnformatted(
          "Editing draft; preview updates when the field is committed.");
    if (!readable_preview_error_.empty()) {
      ImGui::TextWrapped("%s", readable_preview_error_.c_str());
      if (readable_preview_)
        ImGui::Text("Stale preview: showing last valid page %zu.",
                    readable_last_good_page_ + 1);
      else
        ImGui::TextUnformatted("Preview unavailable; repair the text above.");
    }
    if (readable_preview_ && readable_preview_->reader_panel) {
      const auto bounds = *readable_preview_->reader_panel;
      const ImVec2 size{bounds.right - bounds.left, bounds.bottom - bounds.top};
      ImGui::BeginChild("Reader canvas", {0, 0}, ImGuiChildFlags_Borders,
                        ImGuiWindowFlags_HorizontalScrollbar);
      const auto origin = ImGui::GetCursorScreenPos();
      auto* draw = ImGui::GetWindowDrawList();
      draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y},
                          IM_COL32(5, 5, 5, 255));
      for (const auto& line : readable_preview_->reader_lines) {
        auto* font = ImGui::GetFont();
        const float ascent = font->GetFontBaked(line.pixels)->Ascent;
        draw->AddText(font, line.pixels,
                      {origin.x + line.x - bounds.left,
                       origin.y + line.baseline - bounds.top - ascent},
                      IM_COL32(255, 255, 255, 255), line.text.c_str());
      }
      ImGui::Dummy(size);
      ImGui::EndChild();
    }
  }
  ImGui::End();
}
