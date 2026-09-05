#ifndef EDITOR_EDITOR_PROPERTY_EDIT_HPP
#define EDITOR_EDITOR_PROPERTY_EDIT_HPP

#include "editor/editor_document.hpp"

// Widget drags edit this draft; only completion commits one document command.
class EditorPropertyEdit {
 public:
  void synchronize(const EditorDocument& document) {
    if (generation_ != document.revision() || id_ != document.selection()) {
      id_ = document.selection();
      generation_ = document.revision();
      value_ = document.object(id_);
    }
  }
  [[nodiscard]] std::optional<EditorObjectValue>& value() noexcept {
    return value_;
  }
  [[nodiscard]] bool commit(EditorDocument& document) {
    if (!value_ || generation_ != document.revision() ||
        id_ != document.selection()) {
      synchronize(document);
      return false;
    }
    const bool changed = document.replaceObject(id_, *value_);
    value_ = document.object(id_);
    generation_ = document.revision();
    return changed;
  }

 private:
  EditorObjectId id_{};
  std::uint64_t generation_{};
  std::optional<EditorObjectValue> value_{};
};

#endif
