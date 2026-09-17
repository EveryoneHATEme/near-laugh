#include "core/text/caption_font.hpp"

#include "core/text/utf8.hpp"
#include "core/world/level_document.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
// SHA-256 authenticates the one packaged font before the trusted-input parser.
std::string fontHash(std::span<const unsigned char> input) {
  constexpr std::array<std::uint32_t, 64> k{
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  std::array<std::uint32_t, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                 0xa54ff53a, 0x510e527f, 0x9b05688c,
                                 0x1f83d9ab, 0x5be0cd19};
  std::vector<unsigned char> data(input.begin(), input.end());
  data.push_back(0x80);
  while (data.size() % 64 != 56) data.push_back(0);
  const auto bits = std::uint64_t(input.size()) * 8;
  for (int shift = 56; shift >= 0; shift -= 8)
    data.push_back(static_cast<unsigned char>(bits >> shift));
  for (std::size_t block = 0; block < data.size(); block += 64) {
    std::array<std::uint32_t, 64> w{};
    for (unsigned i = 0; i < 16; ++i)
      for (unsigned j = 0; j < 4; ++j)
        w[i] = (w[i] << 8) | data[block + i * 4 + j];
    for (unsigned i = 16; i < 64; ++i) {
      const auto a = w[i - 15], b = w[i - 2];
      w[i] = w[i - 16] + (std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3)) +
             w[i - 7] + (std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10));
    }
    auto [a, b, c, d, e, f, g, v] = h;
    for (unsigned i = 0; i < 64; ++i) {
      const auto t1 = v +
                      (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25)) +
                      ((e & f) ^ (~e & g)) + k[i] + w[i];
      const auto t2 = (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22)) +
                      ((a & b) ^ (a & c) ^ (b & c));
      v = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    const std::array result{a, b, c, d, e, f, g, v};
    for (unsigned i = 0; i < 8; ++i) h[i] += result[i];
  }
  std::string result;
  for (auto word : h)
    for (int shift = 28; shift >= 0; shift -= 4)
      result += "0123456789abcdef"[(word >> shift) & 15];
  return result;
}
constexpr std::array<float, 4> sizes{24, 32, 48, 64};

std::string utf8Line(std::span<const char32_t> scalars) {
  std::string text;
  for (const auto scalar : scalars) {
    if (scalar < 0x80) {
      text.push_back(static_cast<char>(scalar));
    } else if (scalar < 0x800) {
      text.push_back(static_cast<char>(0xc0 | (scalar >> 6)));
      text.push_back(static_cast<char>(0x80 | (scalar & 63)));
    } else if (scalar < 0x10000) {
      text.push_back(static_cast<char>(0xe0 | (scalar >> 12)));
      text.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 63)));
      text.push_back(static_cast<char>(0x80 | (scalar & 63)));
    } else {
      text.push_back(static_cast<char>(0xf0 | (scalar >> 18)));
      text.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 63)));
      text.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 63)));
      text.push_back(static_cast<char>(0x80 | (scalar & 63)));
    }
  }
  return text;
}
}  // namespace

CaptionFont::CaptionFont(const std::filesystem::path& root) {
  const auto path = root / "fonts/NotoSans-Regular.ttf";
  try {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() <= 0 || file.tellg() > 2 * 1024 * 1024)
      throw std::runtime_error("missing, empty or oversized trusted font");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(file.tellg()));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
      throw std::runtime_error("cannot read font");
    if (fontHash(bytes) !=
        "b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5")
      throw std::runtime_error(
          "font content hash does not match the pinned Noto Sans resource");
    stbtt_fontinfo font{};
    if (!stbtt_InitFont(&font, bytes.data(),
                        stbtt_GetFontOffsetForIndex(bytes.data(), 0)))
      throw std::runtime_error("cannot parse pinned font");
    std::vector<int> repertoire;
    for (int c = 32; c <= 126; ++c) repertoire.push_back(c);
    for (int c = 0x400; c <= 0x45f; ++c) repertoire.push_back(c);
    for (int c : {0xab, 0xbb, 0x2013, 0x2014, 0x2018, 0x2019, 0x201c, 0x201d,
                  0x2026, 0x2116})
      repertoire.push_back(c);
    for (int c : repertoire)
      if (stbtt_FindGlyphIndex(&font, c) == 0)
        throw std::runtime_error("font lacks required glyph");
    atlas_.resize(atlas_size * atlas_size);
    std::array<std::vector<stbtt_packedchar>, 4> packed;
    std::array<stbtt_pack_range, 4> ranges{};
    for (unsigned i = 0; i < 4; ++i) {
      packed[i].resize(repertoire.size());
      ranges[i].font_size = sizes[i];
      ranges[i].array_of_unicode_codepoints = repertoire.data();
      ranges[i].num_chars = static_cast<int>(repertoire.size());
      ranges[i].chardata_for_range = packed[i].data();
    }
    stbtt_pack_context context{};
    if (!stbtt_PackBegin(&context, atlas_.data(), atlas_size, atlas_size, 0, 2,
                         nullptr))
      throw std::runtime_error("cannot allocate caption atlas packer");
    const bool packed_ok =
        stbtt_PackFontRanges(&context, bytes.data(), 0, ranges.data(), 4) != 0;
    stbtt_PackEnd(&context);
    if (!packed_ok)
      throw std::runtime_error("caption font exceeds bounded atlas");
    for (unsigned i = 0; i < 4; ++i) {
      for (std::size_t j = 0; j < repertoire.size(); ++j) {
        const auto& p = packed[i][j];
        glyphs_[i].push_back(
            {static_cast<char32_t>(repertoire[j]), p.xadvance, p.xoff, p.yoff,
             p.xoff2, p.yoff2, float(p.x0) / atlas_size,
             float(p.y0) / atlas_size, float(p.x1) / atlas_size,
             float(p.y1) / atlas_size});
      }
    }
    // Reserved solid texel for backing panels, outside the packer's padding.
    atlas_[0] = 255;
    font_bytes_ = std::move(bytes);
  } catch (const std::exception& error) {
    const auto native = path.u8string();
    throw std::runtime_error("Caption font '" +
                             std::string(native.begin(), native.end()) +
                             "': " + error.what());
  }
}
const GlyphMetrics& CaptionFont::glyph(char32_t scalar, unsigned size) const {
  for (const auto& glyph : glyphs_[size])
    if (glyph.scalar == scalar) return glyph;
  throw std::invalid_argument("caption contains unsupported glyph U+" +
                              std::to_string(static_cast<unsigned>(scalar)));
}
CaptionLayout CaptionFont::layout(CaptionPresentation text, unsigned width,
                                  unsigned height,
                                  HouseholdTextPresentation household) const {
  CaptionLayout result;
  if (!width || !height) return result;
  const float scale = std::min(float(width) / 800, float(height) / 600);
  const unsigned size = scale >= 2.66F   ? 3
                        : scale >= 2     ? 2
                        : scale >= 1.33F ? 1
                                         : 0;
  const float pixels = sizes[size], line_height = pixels * 1.35F;
  const float margin = width * .05F, padding = pixels * .5F;
  const float available = std::max(1.0F, width - margin * 2 - padding * 2);
  const auto quad = [&](float x0, float y0, float x1, float y1, float u0,
                        float v0, float u1, float v1,
                        std::array<std::uint8_t, 4> color) {
    if (result.vertices.size() + 6 > maximum_vertices)
      throw std::invalid_argument("combined game text exceeds vertex bound");
    x0 = std::clamp(x0, 0.0F, float(width));
    x1 = std::clamp(x1, 0.0F, float(width));
    y0 = std::clamp(y0, 0.0F, float(height));
    y1 = std::clamp(y1, 0.0F, float(height));
    const auto vertex = [&](float x, float y, float u, float v) {
      return TextVertex{2 * x / width - 1, 2 * y / height - 1, u, v, color};
    };
    const auto a = vertex(x0, y0, u0, v0), b = vertex(x1, y0, u1, v0),
               c = vertex(x1, y1, u1, v1), d = vertex(x0, y1, u0, v1);
    result.vertices.insert(result.vertices.end(), {a, b, c, a, c, d});
  };
  const auto lane = [&](ResolvedCaption caption, bool ambience) -> unsigned {
    if (caption.text.empty() && caption.label.empty()) return 0;
    auto label = captionScalars(caption.label),
         body = captionScalars(caption.text);
    if (label.size() + body.size() > 160 || body.empty())
      throw std::invalid_argument("caption label/text exceeds bounded profile");
    if (!label.empty()) {
      label.push_back(U':');
      label.push_back(U' ');
    }
    label.insert(label.end(), body.begin(), body.end());
    std::vector<std::vector<char32_t>> lines(1);
    float used = 0;
    for (std::size_t i = 0; i < label.size();) {
      std::size_t end = i;
      while (end < label.size() && label[end] != U' ') ++end;
      if (end == i) ++end;
      float word_width = 0;
      for (auto j = i; j < end; ++j)
        word_width += glyph(label[j], size).advance;
      if (used > 0 && used + word_width > available) {
        lines.emplace_back();
        used = 0;
      }
      for (; i < end; ++i) {
        const auto c = label[i];
        const auto advance = glyph(c, size).advance;
        if (used == 0 && c == U' ') continue;
        if (used > 0 && used + advance > available) {
          lines.emplace_back();
          used = 0;
        }
        lines.back().push_back(c);
        used += advance;
      }
    }
    const unsigned limit = ambience ? 2 : 4;
    if (lines.size() > limit && width >= 800 && height >= 600)
      throw std::invalid_argument(
          "caption cannot fit its supported display lane");
    if (lines.size() > limit)
      lines.resize(limit);  // Bounded best effort below the supported extent.
    const float bottom =
        height * .95F - (ambience ? 4 * line_height + 3 * padding : 0);
    const float top = bottom - float(lines.size()) * line_height - 2 * padding;
    const float white = .5F / atlas_size;
    quad(margin, top, width - margin, bottom, white, white, white, white,
         {5, 5, 5, 225});
    for (std::size_t row = 0; row < lines.size(); ++row) {
      float x = margin + padding;
      const float baseline = top + padding + pixels + row * line_height;
      for (const auto scalar : lines[row]) {
        const auto& g = glyph(scalar, size);
        if (scalar != U' ')
          quad(x + g.left, baseline + g.top, x + g.right, baseline + g.bottom,
               g.u0, g.v0, g.u1, g.v1, {255, 255, 255, 255});
        x += g.advance;
      }
    }
    return static_cast<unsigned>(lines.size());
  };
  result.foreground_lines = lane(text.foreground, false);
  result.ambience_lines = lane(text.ambience, true);
  if (household.reader.title.empty() && household.reader.page.empty() &&
      household.reader.controls.empty() && household.hint.empty() &&
      household.feedback.empty())
    return result;

  // Use the minimum supported canvas below 800x600; quad() then clips bounded
  // best-effort geometry. No division or allocation depends on a tiny extent.
  const float canvas_width = float(std::max(width, 800U));
  const float canvas_height = float(std::max(height, 600U));
  const float ui_scale = std::min(canvas_width / 800, canvas_height / 600);
  const float left = canvas_width * .05F, right = canvas_width * .95F;
  const float inset = 6 * ui_scale, gap = 2 * ui_scale;
  // Always reserve BOTH complete caption lanes, even if currently empty. The
  // extra pixel covers the existing caption atlas thresholds at 1.33/2.66.
  const float caption_top =
      canvas_height * .95F -
      std::max(6 * line_height + 5 * padding, 255.4F * ui_scale);
  const float hint_pixels = 14 * ui_scale;
  const float hint_line_height = hint_pixels * 1.25F;
  const float band_height = 2 * hint_line_height + 4 * ui_scale;
  const float band_gap = 6 * ui_scale;
  const TextPanelBounds feedback_panel{left, caption_top - band_height, right,
                                       caption_top};
  const TextPanelBounds hint_panel{left,
                                   feedback_panel.top - band_gap - band_height,
                                   right, feedback_panel.top - band_gap};
  const TextPanelBounds reader_panel{left, canvas_height * .05F, right,
                                     hint_panel.top - band_gap};
  const float line_width = right - left - 2 * inset;
  // Household wrapping and drawing share one existing atlas size. Scaling
  // these metrics continuously keeps a minimum-size preflight valid across
  // caption bucket changes and never rebakes the font during resize.
  constexpr unsigned household_size = 3;

  using Lines = std::vector<std::vector<char32_t>>;
  const auto wrap = [&](std::string_view value, unsigned scalar_limit,
                        bool allow_newlines, float draw_pixels,
                        std::string_view field) -> Lines {
    const auto fail = [&](std::string_view reason) {
      throw std::invalid_argument(std::string(field) + ": " +
                                  std::string(reason));
    };
    std::vector<char32_t> scalars;
    try {
      scalars = readableScalars(value, allow_newlines);
    } catch (const std::invalid_argument& error) {
      fail(error.what());
    }
    if (scalars.size() > scalar_limit)
      fail("exceeds " + std::to_string(scalar_limit) + " Unicode scalars");
    const float glyph_scale = draw_pixels / sizes[household_size];
    const auto metrics = [&](char32_t scalar) -> const GlyphMetrics& {
      try {
        return glyph(scalar, household_size);
      } catch (const std::invalid_argument& error) {
        fail(error.what());
        throw;
      }
    };
    Lines lines(1);
    float used = 0;
    for (std::size_t i = 0; i < scalars.size();) {
      if (scalars[i] == U'\n') {
        lines.emplace_back();
        used = 0;
        ++i;
        continue;
      }
      if (scalars[i] == U' ') {
        const float advance = metrics(U' ').advance * glyph_scale;
        if (used + advance <= line_width) {
          lines.back().push_back(U' ');
          used += advance;
        }
        ++i;
        continue;
      }
      std::size_t end = i;
      float word_advance = 0, word_right = 0;
      while (end < scalars.size() && scalars[end] != U' ' &&
             scalars[end] != U'\n') {
        const auto& g = metrics(scalars[end++]);
        word_right = std::max(word_right, word_advance + g.right * glyph_scale);
        word_advance += g.advance * glyph_scale;
      }
      const float word_width = std::max(word_advance, word_right);
      if (word_width > line_width)
        fail(
            "word is too wide at the supported minimum; insert a space or "
            "shorten the word");
      if (used > 0 && used + word_width > line_width) {
        while (!lines.back().empty() && lines.back().back() == U' ')
          lines.back().pop_back();
        lines.emplace_back();
        used = 0;
      }
      lines.back().insert(lines.back().end(), scalars.begin() + i,
                          scalars.begin() + end);
      used += word_advance;
      i = end;
    }
    return lines;
  };
  const float white = .5F / atlas_size;
  const auto panel = [&](TextPanelBounds bounds) {
    quad(bounds.left, bounds.top, bounds.right, bounds.bottom, white, white,
         white, white, {5, 5, 5, 240});
  };
  const auto draw_lines = [&](const Lines& lines, float top, float draw_pixels,
                              bool preview) {
    const float glyph_scale = draw_pixels / sizes[household_size];
    for (std::size_t row = 0; row < lines.size(); ++row) {
      float x = left + inset;
      const float baseline = top + draw_pixels + row * draw_pixels * 1.25F;
      if (preview)
        result.reader_lines.push_back(
            {utf8Line(lines[row]), x, baseline, draw_pixels});
      for (const auto scalar : lines[row]) {
        const auto& g = glyph(scalar, household_size);
        if (scalar != U' ')
          quad(x + g.left * glyph_scale, baseline + g.top * glyph_scale,
               x + g.right * glyph_scale, baseline + g.bottom * glyph_scale,
               g.u0, g.v0, g.u1, g.v1, {255, 255, 255, 255});
        x += g.advance * glyph_scale;
      }
    }
  };
  const auto band = [&](std::string_view value, unsigned limit,
                        TextPanelBounds bounds, std::string_view field) {
    if (value.empty()) return;
    const auto lines = wrap(value, limit, false, hint_pixels, field);
    if (lines.size() > 2)
      throw std::invalid_argument(
          std::string(field) +
          ": cannot fit two reserved lines; shorten the message");
    panel(bounds);
    draw_lines(lines, bounds.top + 2 * ui_scale, hint_pixels, false);
  };
  const auto& reader = household.reader;
  if (!reader.title.empty() || !reader.page.empty() ||
      !reader.controls.empty()) {
    if (reader.title.empty() || reader.page.empty() || reader.controls.empty())
      throw std::invalid_argument(
          "readable title, page and controls must be nonempty");
    const float title_pixels = 18 * ui_scale, page_pixels = 16 * ui_scale;
    const float control_pixels = 12 * ui_scale;
    const auto title = wrap(reader.title, maximum_title_scalars, false,
                            title_pixels, "readable title");
    const auto page = wrap(reader.page, maximum_page_scalars, true, page_pixels,
                           "readable page");
    const auto controls = wrap(reader.controls, maximum_controls_scalars, false,
                               control_pixels, "readable controls");
    if (title.size() > 2)
      throw std::invalid_argument(
          "readable title: cannot fit two lines; shorten the title");
    if (controls.size() > 2)
      throw std::invalid_argument(
          "readable controls: cannot fit two reserved lines");
    const float title_top = reader_panel.top + inset;
    const float page_top =
        title_top + title.size() * title_pixels * 1.25F + gap;
    // Reserve the worst valid two-line footer even when today's page has a
    // shorter page number or control string. Navigation cannot move the page.
    const float controls_top =
        reader_panel.bottom - inset - 2 * control_pixels * 1.25F;
    if (page_top + page.size() * page_pixels * 1.25F + gap > controls_top)
      throw std::invalid_argument(
          "readable page: cannot fit above the reserved controls/caption "
          "lanes; "
          "shorten the page or split it into authored pages");
    panel(reader_panel);
    result.reader_panel = reader_panel;
    draw_lines(title, title_top, title_pixels, true);
    draw_lines(page, page_top, page_pixels, true);
    draw_lines(controls, controls_top, control_pixels, true);
  }
  band(household.hint, maximum_hint_scalars, hint_panel, "household hint");
  band(household.feedback, maximum_feedback_scalars, feedback_panel,
       "household feedback");
  return result;
}
void CaptionFont::validate(ResolvedCaption caption, bool ambience) const {
  CaptionPresentation text;
  (ambience ? text.ambience : text.foreground) = caption;
  static_cast<void>(layout(text, 800, 600));
}

void CaptionFont::validateReadable(std::string_view title,
                                   std::string_view page) const {
  // Maximum scalar count, with wide supported glyphs and a legal word break.
  // Layout reserves two footer lines independently of the actual controls.
  const std::string controls =
      std::string(47, 'W') + " " + std::string(48, 'W');
  static_cast<void>(layout({}, 800, 600, {{title, page, controls}, {}, {}}));
}

void validateHouseholdText(const LevelHousehold& household,
                           const CaptionFont& font) {
  for (const auto& document : household.documents) {
    for (std::size_t page = 0; page < document.pages.size(); ++page) {
      try {
        font.validateReadable(document.title, document.pages[page]);
      } catch (const std::invalid_argument& error) {
        throw std::invalid_argument("Document '" + document.id + "', page " +
                                    std::to_string(page + 1) + ": " +
                                    error.what());
      }
    }
  }
}
