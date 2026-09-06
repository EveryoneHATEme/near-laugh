#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <locale>
#include <string>
#include <string_view>
#include <vector>

#include "core/world/level_document.hpp"
#include "core/world/light_switch.hpp"
#include "prototype_level_fixture.hpp"

namespace {
std::filesystem::path testDirectory(std::string_view name) {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("near_laugh_level_document_" + std::string(name));
  std::filesystem::remove_all(path);
  std::filesystem::create_directories(path);
  return path;
}

std::string readBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void writeBytes(const std::filesystem::path& path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void replaceOnce(std::string& text, std::string_view from,
                 std::string_view to) {
  const std::size_t position = text.find(from);
  ASSERT_NE(position, std::string::npos);
  text.replace(position, from.size(), to);
}

bool hasField(const std::vector<LevelDiagnostic>& diagnostics,
              std::string_view field) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [field](const LevelDiagnostic& diagnostic) {
                       return diagnostic.document_path.find(field) !=
                              std::string::npos;
                     });
}

void expectPositionEqual(const WorldPosition& actual,
                         const WorldPosition& expected) {
  EXPECT_FLOAT_EQ(actual.x, expected.x);
  EXPECT_FLOAT_EQ(actual.y, expected.y);
  EXPECT_FLOAT_EQ(actual.z, expected.z);
}

void expectDocumentEqual(const LevelDocument& actual,
                         const LevelDocument& expected) {
  EXPECT_EQ(actual.version, expected.version);
  EXPECT_EQ(actual.light_switch, expected.light_switch);
  expectPositionEqual(actual.terrain->origin, expected.terrain->origin);
  EXPECT_FLOAT_EQ(actual.terrain->sample_spacing,
                  expected.terrain->sample_spacing);
  EXPECT_EQ(actual.terrain->heights, expected.terrain->heights);
  ASSERT_EQ(actual.solids.size(), expected.solids.size());
  for (std::size_t index = 0; index < actual.solids.size(); ++index) {
    const PrototypeSolid& left = actual.solids[index];
    const PrototypeSolid& right = expected.solids[index];
    expectPositionEqual(left.center, right.center);
    EXPECT_FLOAT_EQ(left.half_extent.x, right.half_extent.x);
    EXPECT_FLOAT_EQ(left.half_extent.y, right.half_extent.y);
    EXPECT_FLOAT_EQ(left.half_extent.z, right.half_extent.z);
    EXPECT_EQ(left.color, right.color);
    EXPECT_EQ(left.kind, right.kind);
    EXPECT_EQ(left.surface, right.surface);
  }
  expectPositionEqual(actual.entries.front().pose.foot_position,
                      expected.entries.front().pose.foot_position);
  EXPECT_FLOAT_EQ(actual.entries.front().pose.yaw_degrees,
                  expected.entries.front().pose.yaw_degrees);
  for (std::size_t index = 0; index < prototype_point_light_count; ++index) {
    const PrototypePointLight& left =
        actual.environment_light.point_lights[index];
    const PrototypePointLight& right =
        expected.environment_light.point_lights[index];
    expectPositionEqual(left.position, right.position);
    EXPECT_EQ(left.color, right.color);
    EXPECT_FLOAT_EQ(left.intensity, right.intensity);
    EXPECT_FLOAT_EQ(left.radius, right.radius);
  }
  EXPECT_FLOAT_EQ(actual.environment_light.ambient_intensity,
                  expected.environment_light.ambient_intensity);
  expectPositionEqual(actual.static_prop.translation,
                      expected.static_prop.translation);
  EXPECT_FLOAT_EQ(actual.static_prop.yaw_degrees,
                  expected.static_prop.yaw_degrees);
  EXPECT_FLOAT_EQ(actual.static_prop.uniform_scale,
                  expected.static_prop.uniform_scale);
  EXPECT_EQ(actual.static_prop.surface, expected.static_prop.surface);
  expectPositionEqual(actual.static_prop.box_proxy_center,
                      expected.static_prop.box_proxy_center);
  EXPECT_FLOAT_EQ(actual.static_prop.box_proxy_half_extent.x,
                  expected.static_prop.box_proxy_half_extent.x);
  EXPECT_FLOAT_EQ(actual.static_prop.box_proxy_half_extent.y,
                  expected.static_prop.box_proxy_half_extent.y);
  EXPECT_FLOAT_EQ(actual.static_prop.box_proxy_half_extent.z,
                  expected.static_prop.box_proxy_half_extent.z);
}

class CommaDecimalPoint final : public std::numpunct<char> {
 protected:
  char do_decimal_point() const override { return ','; }
};
}  // namespace

TEST(LevelDocument, FixedProfileAndPackagedAssetMatchCurrentSceneExactly) {
  static_assert(level_format_version == 4);
  static_assert(prototype_terrain_sample_count == 97);
  static_assert(level_maximum_solid_count == 240);
  static_assert(prototype_point_light_count == 2);
  const LevelDocumentLoadResult loaded =
      loadLevelDocument(packagedPrototypeLevelPath());
  ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  expectDocumentEqual(*loaded.document, prototypeLevelDocument());
  const std::filesystem::path root = testDirectory("packaged_canonical");
  const std::filesystem::path resaved = root / "prototype.level.json";
  ASSERT_TRUE(saveLevelDocument(resaved, *loaded.document));
  EXPECT_EQ(readBytes(resaved), readBytes(packagedPrototypeLevelPath()));
  std::filesystem::remove_all(root);

  const PrototypeLevel runtime = loadPackagedPrototypeLevel();
  EXPECT_EQ(runtime.terrain()->heights, loaded.document->terrain->heights);
  EXPECT_EQ(runtime.solids().size(), loaded.document->solids.size());
  EXPECT_EQ(runtime.environmentLight().point_lights.size(), 2U);
  EXPECT_TRUE(prototypeLevelIsValid(runtime));
}

TEST(LevelDocument, ValidationIsFieldAwareAndAuthoritative) {
  LevelDocument document = prototypeLevelDocument();
  document.terrain->heights[1] = 100.0F;
  document.solids[0].half_extent.x = 0.0F;
  document.entries.front().pose.foot_position = document.solids[4].center;
  document.environment_light.point_lights[0].radius = 0.0F;
  document.static_prop.box_proxy_half_extent.z = 0.0F;
  const std::vector<LevelDiagnostic> diagnostics =
      validateLevelDocument(document, "invalid.level.json");
  EXPECT_TRUE(hasField(diagnostics, "terrain.cells"));
  EXPECT_TRUE(hasField(diagnostics, "solids[0].half_extent"));
  EXPECT_TRUE(hasField(diagnostics, "entries[0]"));
  EXPECT_TRUE(
      hasField(diagnostics, "environment_light.point_lights[0].radius"));
  EXPECT_TRUE(hasField(diagnostics, "static_prop.box_proxy.half_extent"));
  EXPECT_THROW(static_cast<void>(makePrototypeLevel(document)),
               std::invalid_argument);
}

TEST(LevelDocument, EnforcesSolidCapAndFiniteSupportedValues) {
  LevelDocument document = prototypeLevelDocument();
  while (document.solids.size() < level_maximum_solid_count) {
    document.solids.push_back(document.solids[4]);
  }
  EXPECT_TRUE(validateLevelDocument(document).empty());
  document.solids.push_back(document.solids[4]);
  EXPECT_TRUE(hasField(validateLevelDocument(document), "solids"));

  document = prototypeLevelDocument();
  document.entries.front().pose.yaw_degrees =
      std::numeric_limits<float>::quiet_NaN();
  EXPECT_TRUE(
      hasField(validateLevelDocument(document), "entries[0].yaw_degrees"));
  document = prototypeLevelDocument();
  document.static_prop.translation.x = 1000.0F;
  EXPECT_TRUE(validateLevelDocument(document).empty());
}

TEST(LevelDocument, StrictParserRejectsMalformedUnsupportedAndUnknownShapes) {
  const std::string canonical = readBytes(packagedPrototypeLevelPath());
  const std::filesystem::path root = testDirectory("strict_parse");
  struct Case {
    const char* name;
    std::string bytes;
    std::string expected_field;
  };
  std::vector<Case> cases;
  cases.push_back({"malformed", "{", "byte"});

  std::string version_one = canonical;
  replaceOnce(version_one, "\"version\": 4", "\"version\": 1");
  cases.push_back({"version_one", std::move(version_one), "version"});

  std::string unknown = canonical;
  replaceOnce(unknown, "{\n  \"version\"",
              "{\n  \"model_path\": \"chair.glb\",\n  \"version\"");
  cases.push_back({"path", std::move(unknown), "model_path"});

  std::string missing = canonical;
  replaceOnce(missing, "  \"version\": 4,\n", "");
  cases.push_back({"missing", std::move(missing), "version"});

  std::string invalid_heights = canonical;
  const std::size_t heights = invalid_heights.find("    \"heights\": [\n");
  ASSERT_NE(heights, std::string::npos);
  const std::size_t first_line = invalid_heights.find('\n', heights) + 1;
  const std::size_t second_line = invalid_heights.find('\n', first_line) + 1;
  invalid_heights.erase(first_line, second_line - first_line);
  cases.push_back(
      {"height_count", std::move(invalid_heights), "terrain.heights"});

  std::string invalid_enum = canonical;
  replaceOnce(invalid_enum, "\"kind\": \"boundary\"", "\"kind\": \"door\"");
  cases.push_back({"enum", std::move(invalid_enum), "solids[0].kind"});

  std::string removed_kind = canonical;
  replaceOnce(removed_kind, "\"kind\": \"boundary\"",
              "\"kind\": \"shooting_target\"");
  cases.push_back({"removed_kind", std::move(removed_kind), "solids[0].kind"});

  std::string removed_surface = canonical;
  replaceOnce(removed_surface, "\"surface\": \"boundary\"",
              "\"surface\": \"shooting_target\"");
  cases.push_back(
      {"removed_surface", std::move(removed_surface), "solids[0].surface"});

  std::string excessive = canonical;
  const std::size_t solids_member = excessive.find("  \"solids\": [");
  ASSERT_NE(solids_member, std::string::npos);
  const std::size_t array_begin = excessive.find('[', solids_member);
  const std::size_t array_end =
      excessive.find("\n  ],\n  \"entries\"", array_begin);
  ASSERT_NE(array_end, std::string::npos);
  std::string too_many = "\n";
  for (std::size_t index = 0; index <= level_maximum_solid_count; ++index) {
    too_many += index == level_maximum_solid_count ? "    null" : "    null,\n";
  }
  excessive.replace(array_begin + 1, array_end - array_begin - 1, too_many);
  cases.push_back({"solid_limit", std::move(excessive), "solids"});

  for (const Case& test_case : cases) {
    const std::filesystem::path path =
        root / (test_case.name + std::string(".json"));
    writeBytes(path, test_case.bytes);
    const LevelDocumentLoadResult result = loadLevelDocument(path);
    ASSERT_FALSE(result) << test_case.name;
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_NE(formatLevelDiagnostics(result.diagnostics).find(path.string()),
              std::string::npos);
    EXPECT_TRUE(hasField(result.diagnostics, test_case.expected_field))
        << formatLevelDiagnostics(result.diagnostics);
  }
  std::filesystem::remove_all(root);
}

TEST(LevelDocument, RuntimeLoaderRejectsMissingParseAndValidationFailures) {
  const std::filesystem::path root = testDirectory("runtime_failures");
  const std::filesystem::path missing = root / "missing.level.json";
  try {
    static_cast<void>(loadPrototypeLevel(missing));
    FAIL() << "Expected missing level failure";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find(missing.string()),
              std::string::npos);
  }

  const std::filesystem::path malformed = root / "malformed.level.json";
  writeBytes(malformed, "{");
  EXPECT_THROW(static_cast<void>(loadPrototypeLevel(malformed)),
               std::runtime_error);

  std::string unsupported_spawn = readBytes(packagedPrototypeLevelPath());
  const std::size_t spawn_member = unsupported_spawn.find("  \"entries\": [");
  ASSERT_NE(spawn_member, std::string::npos);
  const std::size_t spawn_x =
      unsupported_spawn.find("\"x\": 0.0", spawn_member);
  ASSERT_NE(spawn_x, std::string::npos);
  unsupported_spawn.replace(spawn_x, std::string("\"x\": 0.0").size(),
                            "\"x\": 1000.0");
  const std::filesystem::path invalid = root / "invalid.level.json";
  writeBytes(invalid, unsupported_spawn);
  const LevelDocumentLoadResult editable = loadLevelDocument(invalid);
  ASSERT_TRUE(editable) << formatLevelDiagnostics(editable.diagnostics);
  EXPECT_TRUE(hasField(validateLevelDocument(*editable.document, invalid),
                       "entries[0].foot_position"));
  EXPECT_THROW(static_cast<void>(loadPrototypeLevel(invalid)),
               std::runtime_error);
  std::filesystem::remove_all(root);
}

TEST(LevelDocument, CanonicalSaveRoundTripsBytesUnderNonClassicGlobalLocale) {
  LevelDocument original = prototypeLevelDocument();
  original.terrain->heights[0] = -0.0F;
  original.static_prop.yaw_degrees =
      std::nextafter(original.static_prop.yaw_degrees, 0.0F);
  const std::filesystem::path root = testDirectory("round_trip");
  const std::filesystem::path first = root / "first.level.json";
  const std::filesystem::path second = root / "second.level.json";
  const std::filesystem::path localized = root / "localized.level.json";

  ASSERT_TRUE(saveLevelDocument(first, original));
  const LevelDocumentLoadResult loaded = loadLevelDocument(first);
  ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  expectDocumentEqual(*loaded.document, original);
  ASSERT_TRUE(saveLevelDocument(second, *loaded.document));
  EXPECT_EQ(readBytes(first), readBytes(second));

  const std::locale previous = std::locale();
  std::locale::global(std::locale(previous, new CommaDecimalPoint));
  const LevelDocumentSaveResult localized_result =
      saveLevelDocument(localized, original);
  std::locale::global(previous);
  ASSERT_TRUE(localized_result)
      << formatLevelDiagnostics(localized_result.diagnostics);
  EXPECT_EQ(readBytes(first), readBytes(localized));
  const std::string bytes = readBytes(localized);
  ASSERT_FALSE(bytes.empty());
  EXPECT_EQ(bytes.back(), '\n');
  EXPECT_EQ(bytes.find('\r'), std::string::npos);
  EXPECT_NE(bytes.find("0.5"), std::string::npos);
  std::filesystem::remove_all(root);
}

TEST(LevelDocument, InvalidAndFilesystemFailuresDoNotReplacePriorData) {
  const std::filesystem::path root = testDirectory("atomic_failures");
  const std::filesystem::path destination = root / "level.json";
  writeBytes(destination, "prior bytes\n");

  LevelDocument invalid = prototypeLevelDocument();
  invalid.solids[0].half_extent.x = 0.0F;
  const LevelDocumentSaveResult invalid_result =
      saveLevelDocument(destination, invalid);
  ASSERT_FALSE(invalid_result);
  EXPECT_EQ(readBytes(destination), "prior bytes\n");
  EXPECT_TRUE(hasField(invalid_result.diagnostics, "solids[0].half_extent"));

  const std::filesystem::path missing_parent = root / "missing" / "level.json";
  const LevelDocumentSaveResult unwritable =
      saveLevelDocument(missing_parent, prototypeLevelDocument());
  ASSERT_FALSE(unwritable);
  EXPECT_EQ(unwritable.diagnostics.front().category,
            LevelDiagnosticCategory::Filesystem);

  const std::filesystem::path directory_destination = root / "directory.json";
  std::filesystem::create_directory(directory_destination);
  const LevelDocumentSaveResult replacement =
      saveLevelDocument(directory_destination, prototypeLevelDocument());
  ASSERT_FALSE(replacement);
  EXPECT_EQ(replacement.diagnostics.front().category,
            LevelDiagnosticCategory::Filesystem);
  EXPECT_TRUE(std::filesystem::is_directory(directory_destination));
  std::filesystem::remove_all(root);
}

TEST(LightSwitchWorld, OptionalHandoffBoundsAndFieldValidation) {
  auto document = prototypeLevelDocument();
  document.light_switch.reset();
  EXPECT_FALSE(makePrototypeLevel(document).lightSwitch());
  document.light_switch =
      PrototypeLightSwitch{{0.0F, 1.6F, 1.05F}, 90, 1, false};
  ASSERT_TRUE(validateLevelDocument(document).empty());
  const auto level = makePrototypeLevel(document);
  EXPECT_EQ(level.lightSwitch(), document.light_switch);
  const auto corners = lightSwitchCorners(*document.light_switch);
  for (const auto p : corners) {
    EXPECT_NEAR(std::abs(p.x), light_switch_half_extent.z, 0.000001F);
    EXPECT_NEAR(std::abs(p.z - 1.05F), light_switch_half_extent.x, 0.000001F);
    EXPECT_TRUE(std::isfinite(p.y));
  }
  document.light_switch->point_light_index = 2;
  EXPECT_TRUE(hasField(validateLevelDocument(document),
                       "light_switch.point_light_index"));
  EXPECT_FALSE(lightSwitchIsValid(*document.light_switch));
  document.light_switch = *level.lightSwitch();
  document.light_switch->position.x = document.terrain->origin.x;
  EXPECT_TRUE(validateLevelDocument(document).empty());
  document.light_switch->position.x = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(lightSwitchIsValid(*document.light_switch));
  document.light_switch = *level.lightSwitch();
  document.light_switch->yaw_degrees = std::numeric_limits<float>::quiet_NaN();
  EXPECT_TRUE(
      hasField(validateLevelDocument(document), "light_switch.yaw_degrees"));
  EXPECT_EQ(level.lightSwitch()->point_light_index, 1U);
}

TEST(LevelDocument, SwitchRoundTripsAndVersionTwoNormalizesWithoutRewriting) {
  const auto root = testDirectory("switch_versions");
  const auto path = root / "level.json";
  auto document = prototypeLevelDocument();
  for (const bool present : {false, true}) {
    document.light_switch = present ? std::optional{PrototypeLightSwitch{
                                          {0, 1.6F, 1.05F}, 37.5F, 1, false}}
                                    : std::nullopt;
    ASSERT_TRUE(saveLevelDocument(path, document));
    const auto bytes = readBytes(path);
    const auto loaded = loadLevelDocument(path);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(*loaded.document, document);
    ASSERT_TRUE(saveLevelDocument(path, *loaded.document));
    EXPECT_EQ(readBytes(path), bytes);
  }
  document = prototypeLevelDocument();
  document.light_switch.reset();
  auto old_bytes = readBytes("tests/fixtures/levels/prototype-v3.level.json");
  replaceOnce(old_bytes, "\"version\": 3", "\"version\": 2");
  old_bytes.erase(old_bytes.find(",\n  \"light_switch\""));
  old_bytes += "\n}\n";
  writeBytes(path, old_bytes);
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  EXPECT_EQ(*loaded.document, document);
  EXPECT_EQ(readBytes(path), old_bytes);
  ASSERT_TRUE(saveLevelDocument(path, *loaded.document));
  EXPECT_NE(readBytes(path).find("\"version\": 4"), std::string::npos);
  EXPECT_NE(readBytes(path).find("\"light_switch\": null"), std::string::npos);
  const auto current = readBytes(path);
  ASSERT_TRUE(saveLevelDocument(path, *loadLevelDocument(path).document));
  EXPECT_EQ(readBytes(path), current);
  std::filesystem::remove_all(root);
}

TEST(LevelDocument, SwitchParserRequiresExactShapeAndTypes) {
  const auto root = testDirectory("switch_parse");
  const auto path = root / "level.json";
  auto document = prototypeLevelDocument();
  document.light_switch = PrototypeLightSwitch{{0, 1.6F, 1.05F}, 0, 0, true};
  ASSERT_TRUE(saveLevelDocument(path, document));
  const auto canonical = readBytes(path);
  for (const auto* bad : {"-1", "2", "0.0", "true", "\"0\"", "4294967296"}) {
    auto bytes = canonical;
    replaceOnce(bytes, "\"point_light_index\": 0",
                std::string("\"point_light_index\": ") + bad);
    writeBytes(path, bytes);
    const auto loaded = loadLevelDocument(path);
    EXPECT_FALSE(loaded) << bad;
    EXPECT_TRUE(hasField(loaded.diagnostics, "light_switch.point_light_index"));
  }
  for (const auto* bad : {"0", "1", "null", "\"false\""}) {
    auto bytes = canonical;
    replaceOnce(bytes, "\"initially_on\": true",
                std::string("\"initially_on\": ") + bad);
    writeBytes(path, bytes);
    EXPECT_TRUE(hasField(loadLevelDocument(path).diagnostics,
                         "light_switch.initially_on"));
  }
  for (const auto* replacement : {"\"unknown\": true", "\"version\": true"}) {
    auto bytes = canonical;
    replaceOnce(bytes, "\"initially_on\": true", replacement);
    writeBytes(path, bytes);
    EXPECT_FALSE(loadLevelDocument(path));
  }
  document.light_switch.reset();
  ASSERT_TRUE(saveLevelDocument(path, document));
  const auto absent = readBytes(path);
  for (const auto* bad : {"[]", "{}", "false", "1"}) {
    auto bytes = absent;
    replaceOnce(bytes, "\"light_switch\": null",
                std::string("\"light_switch\": ") + bad);
    writeBytes(path, bytes);
    EXPECT_FALSE(loadLevelDocument(path));
  }
  auto missing = absent;
  replaceOnce(missing, ",\n  \"light_switch\": null", "");
  writeBytes(path, missing);
  EXPECT_TRUE(hasField(loadLevelDocument(path).diagnostics, "light_switch"));
  auto mixed = readBytes("tests/fixtures/levels/prototype-v3.level.json");
  replaceOnce(mixed, "\"version\": 3", "\"version\": 2");
  writeBytes(path, mixed);
  EXPECT_TRUE(hasField(loadLevelDocument(path).diagnostics, "light_switch"));
  std::filesystem::remove_all(root);
}
