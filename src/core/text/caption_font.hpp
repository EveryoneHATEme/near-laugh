#ifndef CORE_TEXT_CAPTION_FONT_HPP
#define CORE_TEXT_CAPTION_FONT_HPP

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/text/presentation.hpp"

struct LevelHousehold;

struct GlyphMetrics {
  char32_t scalar{};
  float advance{}, left{}, top{}, right{}, bottom{};
  float u0{}, v0{}, u1{}, v1{};
};
struct TextVertex {
  float x{}, y{}, u{}, v{};
  std::array<std::uint8_t, 4> color{};
};
// Owned UTF-8 lines and framebuffer coordinates for the editor's reader
// preview. The baseline uses the same trusted font metrics as the game atlas.
struct PositionedReaderLine {
  std::string text;
  float x{}, baseline{}, pixels{};
};
struct TextPanelBounds {
  float left{}, top{}, right{}, bottom{};
};
struct CaptionLayout {
  std::vector<TextVertex> vertices;
  unsigned foreground_lines{}, ambience_lines{};
  std::optional<TextPanelBounds> reader_panel;
  std::vector<PositionedReaderLine> reader_lines;
};
class CaptionFont {
 public:
  explicit CaptionFont(const std::filesystem::path& resource_root);
  [[nodiscard]] CaptionLayout layout(
      CaptionPresentation text, unsigned width, unsigned height,
      HouseholdTextPresentation household = {}) const;
  void validate(ResolvedCaption caption, bool ambience) const;
  void validateReadable(std::string_view title, std::string_view page) const;
  [[nodiscard]] std::span<const std::uint8_t> atlas() const noexcept {
    return atlas_;
  }
  [[nodiscard]] std::span<const std::uint8_t> fontBytes() const noexcept {
    return font_bytes_;
  }
  static constexpr unsigned atlas_size = 2048;
  static constexpr unsigned maximum_vertices = 8192;
  static constexpr unsigned maximum_title_scalars = 80;
  static constexpr unsigned maximum_page_scalars = 480;
  static constexpr unsigned maximum_controls_scalars = 96;
  static constexpr unsigned maximum_hint_scalars = 96;
  static constexpr unsigned maximum_feedback_scalars = 120;

 private:
  [[nodiscard]] const GlyphMetrics& glyph(char32_t scalar, unsigned size) const;
  std::array<std::vector<GlyphMetrics>, 4> glyphs_;
  std::vector<std::uint8_t> atlas_;
  std::vector<std::uint8_t> font_bytes_;
};

// Run after authored-definition validation, before constructing a runtime.
// Failures name the document and the one-based authored page.
void validateHouseholdText(const LevelHousehold& household,
                           const CaptionFont& font);

#endif
