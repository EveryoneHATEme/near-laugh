#ifndef CORE_TEXT_PRESENTATION_HPP
#define CORE_TEXT_PRESENTATION_HPP

#include <string_view>

// Borrowed resolved text only; a renderer never retains these views.
struct ResolvedCaption {
  std::string_view label{};
  std::string_view text{};
};
struct CaptionPresentation {
  ResolvedCaption foreground{};
  ResolvedCaption ambience{};
};

#endif
