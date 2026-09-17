#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "core/text/caption_font.hpp"
#include "core/text/utf8.hpp"
#include "core/world/level_document.hpp"

namespace {
const CaptionFont& householdFont() {
  static const CaptionFont font("resources");
  return font;
}

std::string repeatedText(std::string_view phrase, unsigned scalars) {
  const auto count = readableScalars(phrase).size();
  std::string result;
  while (scalars >= count) {
    result += phrase;
    scalars -= static_cast<unsigned>(count);
  }
  result.append(scalars, 'i');
  return result;
}

constexpr std::array<std::pair<unsigned, unsigned>, 12> supported_sizes{
    {{800, 600},
     {1280, 720},
     {1920, 1080},
     {2560, 1440},
     {3840, 2160},
     {800, 2160},
     {3840, 600},
     {1064, 798},
     {1067, 800},
     {2128, 1596},
     {2134, 1600},
     {1600, 1200}}};

struct MaximumText {
  std::string title = repeatedText("Ёж и ёлка. ", 80);
  std::string page = repeatedText("Ёж идёт. ", 480);
  std::string controls = repeatedText("W ", 96);
  std::string hint = repeatedText("W ", 96);
  std::string feedback = repeatedText("W ", 120);
  std::string foreground = repeatedText("WWWWWWWWW ", 160);
  std::string ambience = repeatedText("rrrrrrrrr ", 160);

  [[nodiscard]] HouseholdTextPresentation household() const {
    return {{title, page, controls}, hint, feedback};
  }
  [[nodiscard]] CaptionPresentation captions() const {
    return {{"", foreground}, {"", ambience}};
  }
};

// Solid backing quads are part of the visible output. Compare their complete
// rectangles, without relying on draw order or a private wrapping algorithm.
std::vector<TextPanelBounds> backingPanels(const CaptionLayout& layout,
                                           unsigned width, unsigned height) {
  std::vector<TextPanelBounds> panels;
  for (std::size_t i = 0; i < layout.vertices.size(); i += 6) {
    const auto& v = layout.vertices[i];
    if (v.color[0] != 5 || v.color[1] != 5 || v.color[2] != 5) continue;
    TextPanelBounds bounds{float(width), float(height), 0, 0};
    for (std::size_t j = 0; j < 6; ++j) {
      const auto& vertex = layout.vertices[i + j];
      const float x = (vertex.x + 1) * width / 2;
      const float y = (vertex.y + 1) * height / 2;
      bounds.left = std::min(bounds.left, x);
      bounds.right = std::max(bounds.right, x);
      bounds.top = std::min(bounds.top, y);
      bounds.bottom = std::max(bounds.bottom, y);
    }
    panels.push_back(bounds);
  }
  return panels;
}
}  // namespace

TEST(HouseholdText,
     MaximumFieldsAndBothFullCaptionLanesFitEverySupportedProfile) {
  const auto& font = householdFont();
  const MaximumText text;
  const auto atlas = std::vector(font.atlas().begin(), font.atlas().end());
  const auto* bytes = font.fontBytes().data();
  ASSERT_NO_THROW(font.validateReadable(text.title, text.page));
  ASSERT_EQ(readableScalars(text.title).size(), 80U);
  ASSERT_EQ(readableScalars(text.page).size(), 480U);
  ASSERT_EQ(readableScalars(text.controls).size(), 96U);
  ASSERT_EQ(readableScalars(text.hint).size(), 96U);
  ASSERT_EQ(readableScalars(text.feedback).size(), 120U);
  const auto minimum = font.layout(text.captions(), 800, 600, text.household());
  EXPECT_EQ(minimum.foreground_lines, 4U);
  EXPECT_EQ(minimum.ambience_lines, 2U);

  for (const auto [width, height] : supported_sizes) {
    SCOPED_TRACE(std::to_string(width) + "x" + std::to_string(height));
    const auto captions = font.layout(text.captions(), width, height);
    const auto combined =
        font.layout(text.captions(), width, height, text.household());
    ASSERT_LE(combined.vertices.size(), CaptionFont::maximum_vertices);
    EXPECT_EQ(combined.vertices.size() % 6, 0U);
    ASSERT_TRUE(combined.reader_panel.has_value());
    ASSERT_FALSE(combined.reader_lines.empty());
    // Household text leaves every pre-existing caption vertex unchanged.
    ASSERT_LE(captions.vertices.size(), combined.vertices.size());
    for (std::size_t i = 0; i < captions.vertices.size(); ++i) {
      EXPECT_EQ(captions.vertices[i].x, combined.vertices[i].x);
      EXPECT_EQ(captions.vertices[i].y, combined.vertices[i].y);
      EXPECT_EQ(captions.vertices[i].u, combined.vertices[i].u);
      EXPECT_EQ(captions.vertices[i].v, combined.vertices[i].v);
      EXPECT_EQ(captions.vertices[i].color, combined.vertices[i].color);
    }
    auto panels = backingPanels(combined, width, height);
    ASSERT_EQ(panels.size(), 5U);
    std::sort(panels.begin(), panels.end(),
              [](auto a, auto b) { return a.top < b.top; });
    for (std::size_t i = 1; i < panels.size(); ++i)
      EXPECT_LT(panels[i - 1].bottom, panels[i].top);
    for (const auto& vertex : combined.vertices) {
      EXPECT_TRUE(std::isfinite(vertex.x));
      EXPECT_TRUE(std::isfinite(vertex.y));
      EXPECT_GE(vertex.x, -.901F);
      EXPECT_LE(vertex.x, .901F);
      EXPECT_GE(vertex.y, -.901F);
      EXPECT_LE(vertex.y, .901F);
    }
    float previous_bottom = combined.reader_panel->top;
    for (const auto& line : combined.reader_lines) {
      EXPECT_GE(line.x, combined.reader_panel->left);
      EXPECT_GT(line.baseline, previous_bottom);
      EXPECT_LT(line.baseline + line.pixels * .25F,
                combined.reader_panel->bottom);
      previous_bottom = line.baseline;
      EXPECT_NO_THROW(static_cast<void>(readableScalars(line.text)));
    }
  }
  EXPECT_EQ(atlas, std::vector(font.atlas().begin(), font.atlas().end()));
  EXPECT_EQ(bytes, font.fontBytes().data());
}

TEST(HouseholdText, CombinedActualGeometryCoversMaximumNonspaceScalarCounts) {
  MaximumText text;
  // Dense valid glyphs approach the aggregate bound, including the label
  // punctuation added by both maximum-length caption lanes.
  text.title.assign(80, 'i');
  text.page = repeatedText(std::string(159, 'i') + " ", 480);
  text.controls.assign(96, 'i');
  text.hint.assign(96, 'i');
  text.feedback.assign(120, 'i');
  // The first word shares the label's line. A single unbroken word here
  // would correctly move after the label and exceed the caption lane.
  text.foreground = std::string(38, 'W') + " " + std::string(120, 'W');
  text.ambience = std::string(79, 'r') + " " + std::string(79, 'r');
  const CaptionPresentation captions{{"L", text.foreground},
                                     {"R", text.ambience}};
  const auto layout =
      householdFont().layout(captions, 800, 600, text.household());
  std::size_t visible = 4;  // Two labels and their colons.
  for (const auto* field : {&text.title, &text.page, &text.controls, &text.hint,
                            &text.feedback, &text.foreground, &text.ambience}) {
    const auto scalars = readableScalars(*field);
    visible += std::count_if(scalars.begin(), scalars.end(),
                             [](char32_t c) { return c != U' '; });
  }
  EXPECT_EQ(layout.vertices.size(), 6 * (visible + 5));
  EXPECT_GT(layout.vertices.size(), 7100U);
  EXPECT_EQ(layout.foreground_lines, 4U);
  EXPECT_EQ(layout.ambience_lines, 2U);
  EXPECT_LE(layout.vertices.size(), CaptionFont::maximum_vertices);
}

TEST(HouseholdText, PreviewPreservesRussianYoLatinAndExplicitPageNewlines) {
  const std::string page = "Ёжик and ёлка.\n\nВторая строка — № 2.";
  const auto layout = householdFont().layout(
      {}, 800, 600,
      {{"Ёлка / Note", page, "1/2  A/D — страницы; E/Escape — закрыть"},
       {},
       {}});
  ASSERT_EQ(layout.reader_lines.size(), 5U);
  EXPECT_EQ(layout.reader_lines[0].text, "Ёлка / Note");
  EXPECT_EQ(layout.reader_lines[1].text, "Ёжик and ёлка.");
  EXPECT_EQ(layout.reader_lines[2].text, "");
  EXPECT_EQ(layout.reader_lines[3].text, "Вторая строка — № 2.");
  EXPECT_EQ(layout.reader_lines[4].text,
            "1/2  A/D — страницы; E/Escape — закрыть");
}

TEST(HouseholdText, InvalidFieldsFailWithFieldAndRepairContextBeforePlay) {
  const auto& font = householdFont();
  for (const auto& page :
       {std::string(481, 'i'), std::string(480, 'W'),
        std::string("Страница\n\n\n\n\n\n\n\n\nконец"), std::string("🙂"),
        std::string("\xc0\x80"), std::string("\xed\xa0\x80"),
        std::string("\xf4\x90\x80\x80"), std::string("\xe2\x82"),
        std::string("text\ttext"), std::string("text\r\ntext")}) {
    try {
      font.validateReadable("Document", page);
      FAIL() << "invalid page accepted";
    } catch (const std::invalid_argument& error) {
      EXPECT_NE(std::string(error.what()).find("readable page"),
                std::string::npos);
    }
  }
  EXPECT_THROW(font.validateReadable(std::string(81, 'i'), "Page"),
               std::invalid_argument);
  EXPECT_THROW(font.validateReadable(std::string(80, 'W'), "Page"),
               std::invalid_argument);
  EXPECT_THROW(font.validateReadable("Title\nTitle", "Page"),
               std::invalid_argument);
  EXPECT_THROW(font.validateReadable("", "Page"), std::invalid_argument);
  EXPECT_THROW(font.validateReadable("Title", ""), std::invalid_argument);
  EXPECT_THROW(font.layout({}, 800, 600,
                           {{"Title", "Page", std::string(97, 'i')}, {}, {}}),
               std::invalid_argument);
  EXPECT_THROW(font.layout({}, 800, 600, {{}, std::string(97, 'i'), {}}),
               std::invalid_argument);
  EXPECT_THROW(font.layout({}, 800, 600, {{}, {}, std::string(121, 'i')}),
               std::invalid_argument);
  EXPECT_THROW(font.layout({}, 800, 600, {{}, "hint\nnew line", {}}),
               std::invalid_argument);
}

TEST(HouseholdText, CaptionControlsAndScalarProfileRemainStrict) {
  EXPECT_THROW(static_cast<void>(captionScalars("one\ntwo")),
               std::invalid_argument);
  EXPECT_THROW(static_cast<void>(captionScalars("one\ttwo")),
               std::invalid_argument);
  EXPECT_THROW(static_cast<void>(readableScalars("one\ntwo")),
               std::invalid_argument);
  EXPECT_EQ(readableScalars("one\ntwo", true).size(), 7U);
  EXPECT_THROW(householdFont().validate({"x", std::string(160, 'i')}, false),
               std::invalid_argument);
}

TEST(HouseholdText,
     DocumentPreflightVisitsEveryPageAndNamesTheAuthoredLocation) {
  LevelHousehold household;
  household.documents.push_back({"first-note", {}, 0, "Ёлка", {"Страница."}});
  household.documents.push_back(
      {"second-note", {}, 0, "Note", {"Page one.", "Page two."}});
  const auto& font = householdFont();
  EXPECT_NO_THROW(validateHouseholdText(household, font));
  household.documents[1].pages[1] = "🙂";
  try {
    validateHouseholdText(household, font);
    FAIL() << "second document's second page was not validated";
  } catch (const std::invalid_argument& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("Document 'second-note', page 2:"),
              std::string::npos);
    EXPECT_NE(message.find("readable page"), std::string::npos);
    EXPECT_NE(message.find("unsupported glyph"), std::string::npos);
  }
}

TEST(HouseholdText, EmptyAndTinyFramebuffersRemainBounded) {
  const auto& font = householdFont();
  EXPECT_TRUE(font.layout({}, 800, 600).vertices.empty());
  EXPECT_FALSE(font.layout({}, 800, 600).reader_panel.has_value());
  const MaximumText text;
  for (const auto [width, height] :
       std::array<std::pair<unsigned, unsigned>, 5>{
           {{0, 600}, {800, 0}, {1, 1}, {40, 300}, {799, 599}}}) {
    const auto layout =
        font.layout(text.captions(), width, height, text.household());
    EXPECT_LE(layout.vertices.size(), CaptionFont::maximum_vertices);
    for (const auto& vertex : layout.vertices) {
      EXPECT_TRUE(std::isfinite(vertex.x));
      EXPECT_TRUE(std::isfinite(vertex.y));
      EXPECT_GE(vertex.x, -1);
      EXPECT_LE(vertex.x, 1);
      EXPECT_GE(vertex.y, -1);
      EXPECT_LE(vertex.y, 1);
    }
  }
}
