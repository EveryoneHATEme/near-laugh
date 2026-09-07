#ifndef CORE_TEXT_CAPTION_FONT_HPP
#define CORE_TEXT_CAPTION_FONT_HPP

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "core/text/presentation.hpp"

struct GlyphMetrics {
  char32_t scalar{};
  float advance{}, left{}, top{}, right{}, bottom{};
  float u0{}, v0{}, u1{}, v1{};
};
struct TextVertex {
  float x{}, y{}, u{}, v{};
  std::array<std::uint8_t, 4> color{};
};
struct CaptionLayout {
  std::vector<TextVertex> vertices;
  unsigned foreground_lines{}, ambience_lines{};
};
class CaptionFont {
 public:
  explicit CaptionFont(const std::filesystem::path& resource_root);
  [[nodiscard]] CaptionLayout layout(CaptionPresentation text, unsigned width,
                                     unsigned height) const;
  void validate(ResolvedCaption caption, bool ambience) const;
  [[nodiscard]] std::span<const std::uint8_t> atlas() const noexcept {
    return atlas_;
  }
  [[nodiscard]] std::span<const std::uint8_t> fontBytes() const noexcept {
    return font_bytes_;
  }
  static constexpr unsigned atlas_size = 2048;
  static constexpr unsigned maximum_vertices = 2000;

 private:
  [[nodiscard]] const GlyphMetrics& glyph(char32_t scalar, unsigned size) const;
  std::array<std::vector<GlyphMetrics>, 4> glyphs_;
  std::vector<std::uint8_t> atlas_;
  std::vector<std::uint8_t> font_bytes_;
};

#endif
