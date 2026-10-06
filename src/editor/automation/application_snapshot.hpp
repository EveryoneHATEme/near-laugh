#pragma once

#include <map>
#include <string>

#include "editor/automation/protocol.hpp"
#include "editor/editor_document.hpp"

namespace editor_automation {
using ObjectRefs = std::map<EditorObjectId, std::string>;

Json knownValue(Json value, std::string_view type);
Json unavailableValue(std::string_view reason, std::string_view availability = "unavailable");
std::string objectRecordType(const EditorObjectValue& value);

// Copies explicit const projections after the real UI commit path. The result
// contains no editor pointers and is retained with its completed-frame stamp.
Json captureApplication(const EditorDocument& document, const ObjectRefs& refs,
                        const Json& preview_fields = Json::object());
// Copied ordered narrative data also identifies ephemeral semantic rows.
Json narrativeEventFields(const NarrativeEventDefinition& event);
}  // namespace editor_automation
