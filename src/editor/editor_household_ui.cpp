#include <imgui.h>
#include "editor/editor_widget_metadata.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <type_traits>

#include "editor/editor_ui.hpp"

namespace {
enum class PageAction { None, Previous, Next, Add, Remove };
}

void EditorUi::drawHouseholdObjects(EditorDocument& document) {
  if (!EditorWidgets::CollapsingHeader("Household", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  const std::array buttons{"Add box", "Add document", "Add radio control"};
  const std::array labels{"Box: ", "Document: ", "Radio control: "};
  const std::array limits{level_maximum_household_box_count,
                          level_maximum_household_document_count,
                          level_maximum_household_radio_count};
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    const auto kind = static_cast<EditorHouseholdKind>(i);
    EditorWidgets::BeginDisabled(document.householdIds(kind).size() >= limits[i]);
    const bool add = EditorWidgets::Button(buttons[i]);
    EditorWidgets::EndDisabled();
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
              EditorWidgetMetadata::Owner owner(id);
              EditorWidgetMetadata::next("select");
              EditorWidgets::PushID(static_cast<int>(id));
              if (EditorWidgets::Selectable((labels[kind] + record.id).c_str(),
                                    document.selection() == id))
                selectObject(document, id);
              EditorWidgets::PopID();
            }
          },
          value);
    }
  EditorWidgets::Separator();
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
                            ? EditorWidgets::InputTextMultiline(label, buffer.data(),
                                                        capacity, {0, 130})
                            : EditorWidgets::InputText(label, buffer.data(), capacity);
    if (edited) value = buffer.data();
    commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
  };
  const auto position = [&](const char* label, WorldPosition& value) {
    float xyz[]{value.x, value.y, value.z};
    if (EditorWidgets::DragFloat3(label, xyz, .05F, 0, 0, "%.3f"))
      value = {xyz[0], xyz[1], xyz[2]};
    commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
  };
  const auto yaw = [&](float& value) {
    EditorWidgetMetadata::next("initial-yaw-degrees", "yaw_degrees", "degrees", "object");
    EditorWidgets::DragFloat("Initial yaw (degrees)", &value, .5F, 0, 0, "%.3f");
    commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
  };
  const auto reference = [&](const char* label, std::string& value,
                             const auto& choices) {
    if (EditorWidgets::BeginCombo(label, value.empty() ? "<missing>" : value.c_str())) {
      for (const auto& choice : choices)
        if (EditorWidgets::Selectable(choice.id.c_str(), value == choice.id)) {
          value = choice.id;
          commit = true;
        }
      EditorWidgets::EndCombo();
    }
    if (std::none_of(choices.begin(), choices.end(),
                     [&](const auto& choice) { return choice.id == value; }))
      EditorWidgets::TextWrapped("Unresolved %s: %s", label,
                         value.empty() ? "<empty>" : value.c_str());
  };
  std::visit(
      [&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, HouseholdBoxDefinition>) {
          EditorWidgets::TextUnformatted("Physical box: 0.30 m cube, 1 kg.");
          EditorWidgets::TextWrapped(
              "Initial pose only. Use Play to test falling, pickup, drop and "
              "throw.");
          EditorWidgetMetadata::next("box-id", "id", "", "object");
          text("Box ID", value.id, 65);
          EditorWidgetMetadata::next("initial-center", "center", "m", "object");
          position("Initial center", value.center);
          yaw(value.yaw_degrees);
        } else if constexpr (std::is_same_v<T, HouseholdDocumentDefinition>) {
          EditorWidgetMetadata::next("document-id", "id", "", "object");
          text("Document ID", value.id, 65);
          EditorWidgetMetadata::next("panel-position", "position", "m", "object");
          position("Panel position", value.position);
          yaw(value.yaw_degrees);
          EditorWidgetMetadata::next("document-title", "title", "", "object");
          text("Document title", value.title,
               level_maximum_document_title_scalars * 4 + 1);
          if (!value.pages.empty()) {
            readable_page_ = std::min(readable_page_, value.pages.size() - 1);
            EditorWidgets::Text("Page %zu of %zu", readable_page_ + 1,
                        value.pages.size());
            EditorWidgetMetadata::Row row("pages", readable_page_);
            EditorWidgets::PushID(static_cast<int>(readable_page_));
            EditorWidgetMetadata::next("page-text", "pages", "", "object");
            text("Page text", value.pages[readable_page_],
                 level_maximum_document_page_scalars * 4 + 1, true);
            EditorWidgets::PopID();
            EditorWidgets::TextWrapped(
                "Enter adds a line; Ctrl+Enter commits the page. Title: 80 "
                "characters; page: 480.");
          } else {
            readable_page_ = 0;
            EditorWidgets::TextUnformatted("No pages. Add a page before Save or Play.");
          }
          EditorWidgets::BeginDisabled(value.pages.empty() || readable_page_ == 0);
          if (EditorWidgets::Button("Previous page"))
            page_action = PageAction::Previous;
          EditorWidgets::EndDisabled();
          EditorWidgets::SameLine();
          EditorWidgets::BeginDisabled(value.pages.empty() ||
                               readable_page_ + 1 >= value.pages.size());
          if (EditorWidgets::Button("Next page")) page_action = PageAction::Next;
          EditorWidgets::EndDisabled();
          EditorWidgets::BeginDisabled(value.pages.size() >=
                               level_maximum_document_page_count);
          if (EditorWidgets::Button("Add page")) page_action = PageAction::Add;
          EditorWidgets::EndDisabled();
          EditorWidgets::SameLine();
          EditorWidgets::BeginDisabled(value.pages.size() <= 1);
          if (EditorWidgets::Button("Remove page")) page_action = PageAction::Remove;
          EditorWidgets::EndDisabled();
        } else if constexpr (std::is_same_v<T, HouseholdRadioDefinition>) {
          EditorWidgets::TextUnformatted(
              "Radio control (separate from its static prop).");
          EditorWidgetMetadata::next("radio-control-id", "id", "", "object");
          text("Radio control ID", value.id, 65);
          EditorWidgetMetadata::next("radio-prop", "prop", "", "object");
          reference("Radio prop", value.prop, level.props);
          EditorWidgetMetadata::next("radio-source", "source", "", "object");
          reference("Radio source", value.source, level.audio.sources);
          EditorWidgetMetadata::next("initially-on", "initially_on", "", "object");
          commit |= EditorWidgets::Checkbox("Initially on", &value.initially_on);
          const auto prop = std::find_if(level.props.begin(), level.props.end(),
                                         [&](const auto& candidate) {
                                           return candidate.id == value.prop;
                                         });
          if (prop != level.props.end() && (prop->model != "apartment-radio" ||
                                            !prop->collision_boxes.empty()))
            EditorWidgets::TextWrapped(
                "Radio prop must use apartment-radio with no collision boxes.");
          EditorWidgets::TextWrapped(
              "Choose an exclusive spatial source with autoplay off and a "
              "captioned ambience loop. Link and ownership errors appear in "
              "Validation.");
          EditorWidgets::BeginDisabled(prop == level.props.end());
          select_prop = EditorWidgets::Button("Select radio prop");
          EditorWidgets::EndDisabled();
        }
      },
      *property_edit_.value());
  const auto error = editorHouseholdFieldError(*property_edit_.value());
  if (!error.empty()) EditorWidgets::TextWrapped("Draft error: %s", error.c_str());
  if (!document.editError().empty())
    EditorWidgets::TextWrapped("Edit rejected: %s", document.editError().c_str());

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
        EditorWidgetMetadata::structureChanged("pages");
        value.pages.emplace_back("Текст новой страницы.");
        if (document.replaceObject(document.selection(), value))
          readable_page_ = value.pages.size() - 1;
        break;
      case PageAction::Remove:
        if (value.pages.size() > 1) {
          EditorWidgetMetadata::structureChanged("pages");
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

EditorReadablePreviewView EditorUi::readablePreview(
    const EditorDocument& document) const {
  EditorReadablePreviewView result;
  const auto selected = document.object(document.selection());
  result.selected = selected &&
      std::holds_alternative<HouseholdDocumentDefinition>(*selected);
  if (!result.selected) return result;
  result.available = readable_preview_.has_value() &&
      readable_object_ == document.selection() &&
      readable_generation_ == document.generation();
  result.stale = result.available && !readable_preview_error_.empty();
  result.page = result.available ? readable_last_good_page_ : readable_page_;
  if (result.available) {
    result.title = readable_preview_title_;
    result.text = readable_preview_text_;
  }
  result.layout_diagnostics = readable_preview_error_;
  if (!document.editError().empty())
    result.validation_diagnostics.push_back(document.editError());
  for (const auto& diagnostic : document.diagnostics())
    if (diagnostic.category == LevelDiagnosticCategory::Validation)
      result.validation_diagnostics.push_back(diagnostic.document_path + ": " +
                                               diagnostic.message);
  return result;
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
      readable_preview_title_ = value.title;
      readable_preview_text_ = value.pages.at(readable_page_);
      readable_preview_error_.clear();
    } catch (const std::exception& error) {
      readable_preview_error_ = "Document '" + value.id + "', page " +
                                std::to_string(readable_page_ + 1) + ": " +
                                error.what();
    }
  }
  EditorWidgets::SetNextWindowPos({340, 475}, ImGuiCond_FirstUseEver);
  EditorWidgets::SetNextWindowSize({780, 315}, ImGuiCond_FirstUseEver);
  if (EditorWidgets::Begin("Readable preview")) {
    EditorWidgets::Text("Minimum framebuffer: 800x600. Page %zu of %zu.",
                readable_page_ + 1, value.pages.size());
    EditorWidgets::TextWrapped(
        "Caption and household hint lanes remain reserved below this panel.");
    if (property_edit_.value() && property_edit_.value() != selected)
      EditorWidgets::TextUnformatted(
          "Editing draft; preview updates when the field is committed.");
    if (!readable_preview_error_.empty()) {
      EditorWidgets::TextWrapped("%s", readable_preview_error_.c_str());
      if (readable_preview_)
        EditorWidgets::Text("Stale preview: showing last valid page %zu.",
                    readable_last_good_page_ + 1);
      else
        EditorWidgets::TextUnformatted("Preview unavailable; repair the text above.");
    }
    if (readable_preview_ && readable_preview_->reader_panel) {
      const auto bounds = *readable_preview_->reader_panel;
      const ImVec2 size{bounds.right - bounds.left, bounds.bottom - bounds.top};
      EditorWidgets::BeginChild("Reader canvas", {0, 0}, ImGuiChildFlags_Borders,
                        ImGuiWindowFlags_HorizontalScrollbar);
      const auto origin = EditorWidgets::GetCursorScreenPos();
      auto* draw = EditorWidgets::GetWindowDrawList();
      draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y},
                          IM_COL32(5, 5, 5, 255));
      for (const auto& line : readable_preview_->reader_lines) {
        auto* font = EditorWidgets::GetFont();
        const float ascent = font->GetFontBaked(line.pixels)->Ascent;
        draw->AddText(font, line.pixels,
                      {origin.x + line.x - bounds.left,
                       origin.y + line.baseline - bounds.top - ascent},
                      IM_COL32(255, 255, 255, 255), line.text.c_str());
      }
      EditorWidgets::Dummy(size);
      EditorWidgets::EndChild();
    }
  }
  EditorWidgets::End();
}
