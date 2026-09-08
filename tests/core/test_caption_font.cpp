#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <fstream>

#include "core/audio/audio_content.hpp"
#include "core/text/caption_font.hpp"
#include "core/world/audio.hpp"

namespace {
const CaptionFont& fixtureFont() {
  static const CaptionFont font("resources");
  return font;
}
}  // namespace

TEST(CaptionFont, PinnedFontBakesRussianYoAndPunctuationIntoAnImmutableAtlas) {
  const auto& font = fixtureFont();
  EXPECT_EQ(font.atlas().size(), 2048U * 2048U);
  EXPECT_GT(std::count_if(font.atlas().begin(), font.atlas().end(),
                          [](auto alpha) { return alpha > 0; }),
            10000);
  const auto layout = font.layout(
      {{"Голос за дверью", "Ёлка, ёж — «пойдём»… № 12!"}, {}}, 800, 600);
  EXPECT_FALSE(layout.vertices.empty());
  EXPECT_LE(layout.foreground_lines, 4U);
  EXPECT_THROW(font.validate({"Голос", "🙂"}, false), std::invalid_argument);
  EXPECT_THROW(font.validate({"Голос", "\xc0\x80"}, false),
               std::invalid_argument);
}

TEST(CaptionFont, EveryPackagedCaptionFitsAtSupportedAndHiDpiFramebufferSizes) {
  LevelAudio audio;
  for (const auto& entry : audioCatalog()) {
    if (entry.id == "character-footstep")
      continue;  // Uncaptioned ambience one-shot.
    audio.cues.push_back(
        {std::string(entry.id), std::string(entry.id), std::string(entry.id),
         entry.id == "radio" ? AudioCueKind::Ambience : AudioCueKind::Essential,
         false, true});
  }
  const auto content = prepareAudioContent("resources", audio);
  const auto& font = fixtureFont();
  for (const auto& track : content.captions)
    for (const auto& segment : track.segments) {
      const ResolvedCaption caption{segment.label, segment.text};
      font.validate(caption, track.id == "radio");
      for (const auto [width, height] :
           std::array<std::pair<unsigned, unsigned>, 7>{{{800, 600},
                                                         {1280, 720},
                                                         {1920, 1080},
                                                         {2560, 1440},
                                                         {3840, 2160},
                                                         {800, 2160},
                                                         {3840, 600}}}) {
        SCOPED_TRACE(track.id + " at " + std::to_string(width) + "x" +
                     std::to_string(height));
        const auto layout = font.layout(
            {caption, {"Радио", "Тихая музыка и помехи."}}, width, height);
        EXPECT_LE(layout.foreground_lines, 4U);
        EXPECT_LE(layout.ambience_lines, 2U);
        EXPECT_LE(layout.vertices.size(), CaptionFont::maximum_vertices);
        for (const auto& v : layout.vertices) {
          EXPECT_GE(v.x, -.91F);
          EXPECT_LE(v.x, .91F);
          EXPECT_GE(v.y, -.91F);
          EXPECT_LE(v.y, .91F);
          EXPECT_GE(v.u, 0);
          EXPECT_LE(v.u, 1);
          EXPECT_GE(v.v, 0);
          EXPECT_LE(v.v, 1);
        }
      }
    }
}

TEST(CaptionFont, EmptySmallAndUnfittableTextStayBounded) {
  const auto& font = fixtureFont();
  EXPECT_TRUE(font.layout({}, 800, 600).vertices.empty());
  EXPECT_TRUE(font.layout({{"Голос", "Привет"}, {}}, 0, 0).vertices.empty());
  const auto tiny = font.layout({{"Голос", "Очень маленькое окно"}, {}}, 1, 1);
  EXPECT_LE(tiny.vertices.size(), CaptionFont::maximum_vertices);
  const std::string excessive(160, 'W');
  EXPECT_THROW(font.validate({"Label", excessive}, false),
               std::invalid_argument);
  EXPECT_THROW(font.validate({{}, excessive}, true), std::invalid_argument);
}

TEST(CaptionFont, MissingAndModifiedFontsFailBeforeParsingWithPathContext) {
  const auto root =
      std::filesystem::temp_directory_path() /
      ("caption-font-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root / "fonts");
  EXPECT_THROW(static_cast<void>(CaptionFont(root)), std::runtime_error);
  std::filesystem::copy_file("resources/fonts/NotoSans-Regular.ttf",
                             root / "fonts/NotoSans-Regular.ttf");
  {
    std::fstream file(root / "fonts/NotoSans-Regular.ttf",
                      std::ios::binary | std::ios::in | std::ios::out);
    file.put('?');
  }
  try {
    static_cast<void>(CaptionFont(root));
    FAIL();
  } catch (const std::runtime_error& e) {
    EXPECT_NE(std::string(e.what()).find("hash"), std::string::npos);
    EXPECT_NE(std::string(e.what()).find("NotoSans-Regular.ttf"),
              std::string::npos);
  }
  std::filesystem::remove_all(root);
}
