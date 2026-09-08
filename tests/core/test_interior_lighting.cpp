#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

#include "core/gameplay/authored_interaction.hpp"
#include "core/render/lighting_resources.hpp"
#include "prototype_level_fixture.hpp"

namespace {
class InteriorLighting : public testing::Test {
 protected:
  void SetUp() override {
    path =
        std::filesystem::temp_directory_path() / "near_laugh_lighting_v8.json";
  }
  void TearDown() override { std::filesystem::remove(path); }
  std::filesystem::path path;
};
std::string read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void write(const std::filesystem::path& path, std::string_view text) {
  std::ofstream output(path, std::ios::binary);
  output << text;
}
LevelDocument capacity() {
  auto doc = prototypeLevelDocument();
  auto& lights = doc.environment_light.point_lights;
  while (lights.size() < 8) {
    auto value = lights[0];
    value.id = "light-" + std::to_string(lights.size());
    lights.push_back(value);
  }
  for (std::size_t i = 0; i < 4; ++i) lights[i].casts_shadows = true;
  doc.light_switches.clear();
  for (std::size_t i = 0; i < 16; ++i)
    doc.light_switches.push_back(
        {{0, 1.6F, 1.05F}, 0, lights[i % 8].id, "switch-" + std::to_string(i)});
  return doc;
}
}  // namespace

TEST_F(InteriorLighting, EmptyAndCapacityCollectionsRoundTripAndKeepOrder) {
  for (bool empty : {false, true}) {
    auto doc = capacity();
    if (empty) {
      doc.environment_light.point_lights.clear();
      doc.light_switches.clear();
    }
    for (float ambient : {0.F, .2F}) {
      doc.environment_light.ambient_intensity = ambient;
      ASSERT_TRUE(validateLevelDocument(doc).empty());
      ASSERT_TRUE(saveLevelDocument(path, doc));
      const auto bytes = read(path);
      const auto loaded = loadLevelDocument(path);
      ASSERT_TRUE(loaded);
      EXPECT_EQ(loaded.source_version, 9U);
      EXPECT_EQ(*loaded.document, doc);
      ASSERT_TRUE(saveLevelDocument(path, *loaded.document));
      EXPECT_EQ(read(path), bytes);
      const auto upload = makePrototypeLightingUpload(doc.environment_light);
      EXPECT_FLOAT_EQ(upload.ambient_intensity[0], ambient);
      EXPECT_EQ(upload.ambient_intensity[1], empty ? 0 : 8);
    }
    std::reverse(doc.environment_light.point_lights.begin(),
                 doc.environment_light.point_lights.end());
    ASSERT_TRUE(saveLevelDocument(path, doc));
    EXPECT_EQ(*loadLevelDocument(path).document, doc);
  }
}

TEST_F(InteriorLighting,
       CountsIdentityLinksShadowBudgetAndFiniteFieldsAreAuthoritative) {
  const auto valid = capacity();
  const auto reject = [&](const auto& doc) {
    EXPECT_FALSE(validateLevelDocument(doc).empty());
    EXPECT_THROW((void)makePrototypeLevel(doc), std::invalid_argument);
    EXPECT_FALSE(saveLevelDocument(path, doc));
  };
  auto doc = valid;
  doc.environment_light.point_lights.push_back(
      valid.environment_light.point_lights[0]);
  reject(doc);
  doc = valid;
  doc.light_switches.push_back(valid.light_switches[0]);
  reject(doc);
  doc = valid;
  doc.environment_light.point_lights[4].casts_shadows = true;
  doc.environment_light.point_lights[4].initially_on = false;
  reject(doc);  // Disabled configured casters still consume their reservation.
  for (const auto& id :
       {std::string{}, std::string("Upper"), std::string("two words"),
        std::string("1light"), std::string("a_b"), std::string(65, 'a')}) {
    doc = valid;
    doc.environment_light.point_lights[7].id = id;
    reject(doc);
    doc = valid;
    doc.light_switches[15].id = id;
    reject(doc);
  }
  doc = valid;
  doc.environment_light.point_lights[7].id =
      doc.environment_light.point_lights[6].id;
  reject(doc);
  doc = valid;
  doc.light_switches[15].id = doc.light_switches[14].id;
  reject(doc);
  for (auto target : {"", "missing"}) {
    doc = valid;
    doc.light_switches[15].light_id = target;
    reject(doc);
  }
  for (float bad : {-1.F, 0.F, std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::quiet_NaN()}) {
    doc = valid;
    doc.environment_light.point_lights[7].radius = bad;
    reject(doc);
    doc = valid;
    doc.environment_light.point_lights[7].intensity = bad;
    reject(doc);
  }
  for (float radius : {.249F, 20.001F}) {
    doc = valid;
    doc.environment_light.point_lights[0].radius = radius;
    reject(doc);
  }
  for (float radius : {.25F, 20.F}) {
    doc = valid;
    doc.environment_light.point_lights[0].radius = radius;
    EXPECT_TRUE(validateLevelDocument(doc).empty());
  }
  doc = valid;
  doc.environment_light.point_lights[7].color = {0, 0, 0};
  EXPECT_TRUE(validateLevelDocument(doc).empty());
  doc.environment_light.point_lights[7].color[0] = -1;
  reject(doc);
  doc = valid;
  doc.environment_light.point_lights[7].intensity =
      std::numeric_limits<float>::max();
  doc.environment_light.point_lights[7].color[0] = 2;
  reject(doc);
  doc = valid;
  doc.environment_light.point_lights[7].radius =
      std::numeric_limits<float>::max();
  reject(doc);
  for (float bad : {-.001F, .201F, std::numeric_limits<float>::quiet_NaN()}) {
    doc = valid;
    doc.environment_light.ambient_intensity = bad;
    reject(doc);
  }
}

TEST_F(InteriorLighting, CurrentCodecRejectsMixedShapesAndNonBooleanFlags) {
  ASSERT_TRUE(saveLevelDocument(path, prototypeLevelDocument()));
  const auto canonical = read(path);
  for (const auto& [from, to] :
       {std::pair{"\"light_switches\"", "\"light_switch\""},
        std::pair{"\"initially_on\": true", "\"initially_on\": 1"},
        std::pair{"\"casts_shadows\": false", "\"casts_shadows\": null"},
        std::pair{"\"light_id\": \"point-light-0\"",
                  "\"point_light_index\": 0"},
        std::pair{"\"version\": 9", "\"version\": 7"}}) {
    auto malformed = canonical;
    const auto at = malformed.find(from);
    ASSERT_NE(at, std::string::npos);
    malformed.replace(at, std::string_view(from).size(), to);
    write(path, malformed);
    EXPECT_FALSE(loadLevelDocument(path));
  }
}

TEST_F(InteriorLighting,
       RetainedV7AudioFixtureMigratesWithoutChangingContentOrSource) {
  const std::filesystem::path source =
      "tests/fixtures/levels/audio-captions-v7.level.json";
  const auto before = read(source);
  const auto legacy = loadLevelDocument(source);
  ASSERT_TRUE(legacy);
  EXPECT_EQ(legacy.source_version, 7U);
  const auto current =
      loadLevelDocument("resources/levels/audio-captions.level.json");
  ASSERT_TRUE(current);
  EXPECT_EQ(*legacy.document, *current.document);
  EXPECT_EQ(read(source), before);
  ASSERT_TRUE(saveLevelDocument(path, *legacy.document));
  EXPECT_EQ(read(path), read("resources/levels/audio-captions.level.json"));
  auto off = before;
  const auto at = off.find("\"initially_on\": true");
  ASSERT_NE(at, std::string::npos);
  off.replace(at, std::string_view("\"initially_on\": true").size(),
              "\"initially_on\": false");
  write(path, off);
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded);
  EXPECT_FALSE(loaded.document->environment_light.point_lights[0].initially_on);
  EXPECT_TRUE(loaded.document->environment_light.point_lights[1].initially_on);
  EXPECT_EQ(loaded.document->audio, legacy.document->audio);
  EXPECT_EQ(loaded.document->doors, legacy.document->doors);
  EXPECT_EQ(loaded.document->light_switches.front().light_id, "point-light-0");
}

TEST(InteriorLightState,
     SharedLinksUnlinkedValuesAndFreshRunRemainIndependent) {
  auto doc = capacity();
  doc.environment_light.point_lights[7].initially_on = false;
  doc.light_switches = {{{0, 1, 0}, 0, "point-light-0", "one"},
                        {{1, 1, 0}, 0, "point-light-0", "two"}};
  const auto before = doc;
  LightSwitchController run(doc.environment_light, doc.light_switches);
  run.toggle(0);
  EXPECT_FALSE(run.pointLightEnabled()[0]);
  run.toggle(1);
  EXPECT_TRUE(run.pointLightEnabled()[0]);
  EXPECT_FALSE(run.pointLightEnabled()[7]);
  run.toggle(1);
  LightSwitchController fresh(doc.environment_light, doc.light_switches);
  EXPECT_TRUE(fresh.pointLightEnabled()[0]);
  EXPECT_FALSE(fresh.pointLightEnabled()[7]);
  EXPECT_EQ(doc, before);
  EXPECT_THROW(run.toggle(2), std::out_of_range);
  doc.environment_light.point_lights.clear();
  doc.light_switches.clear();
  EXPECT_TRUE(LightSwitchController(doc.environment_light, doc.light_switches)
                  .pointLightEnabled()
                  .empty());
}

TEST(InteriorLightState,
     NearestToleranceUsesTrueMinimumAndDurableIdsAcrossOrderings) {
  auto doc = prototypeLevelDocument();
  doc.light_switches = {{{0, 1.6F, 1.05F}, 0, "point-light-0", "z"},
                        {{0, 1.6F, 1.05F - .000075F}, 0, "point-light-1", "b"},
                        {{0, 1.6F, 1.05F - .00015F}, 0, "point-light-0", "a"}};
  for (bool reversed : {false, true}) {
    if (reversed)
      std::reverse(doc.light_switches.begin(), doc.light_switches.end());
    const auto level = makePrototypeLevel(doc);
    PhysicsWorld physics(level);
    DoorController doors(level.doors());
    LightSwitchController lights(level.environmentLight(),
                                 level.lightSwitches());
    AuthoredInteraction interaction;
    const PlayerViewPose view{{0, 1.6F, 2.5F}, {0, 0, -1}};
    (void)interaction.update({}, true, view, level, physics, doors, lights);
    PlayerActionSnapshot input;
    input.interact = true;
    (void)interaction.update(input, true, view, level, physics, doors, lights);
    EXPECT_EQ(lights.pointLightEnabled(), (std::vector<std::uint8_t>{1, 0}));
  }
}

TEST(PointShadowMath, FaceProjectionMatchesRenderingAndCrossEdgeReprojection) {
  for (float radius : {.25F, 20.F})
    for (std::size_t face = 0; face < 6; ++face) {
      const WorldPosition source{2, -3, 4};
      const auto camera =
          pointShadowCamera(source, radius, face).view_projection;
      for (float u : {.01F, .5F, .99F})
        for (float v : {.01F, .5F, .99F}) {
          auto direction = pointShadowDirection(face, u, v);
          direction = {direction.x * radius * .5F, direction.y * radius * .5F,
                       direction.z * radius * .5F};
          const auto projected = projectPointShadow(direction, radius);
          EXPECT_EQ(projected.face, face);
          EXPECT_NEAR(projected.u, u, .000001F);
          EXPECT_NEAR(projected.v, v, .000001F);
          const std::array<float, 4> world{source.x + direction.x,
                                           source.y + direction.y,
                                           source.z + direction.z, 1};
          std::array<float, 4> clip{};
          for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
              clip[row] += camera[col * 4 + row] * world[col];
          EXPECT_NEAR(clip[0] / clip[3] * .5F + .5F, u, .00001F);
          EXPECT_NEAR(clip[1] / clip[3] * .5F + .5F, v, .00001F);
          EXPECT_NEAR(clip[2] / clip[3], projected.depth, .00001F);
        }
      for (float u : {-.001F, 1.001F}) {
        const auto direction = pointShadowDirection(face, u, .5F);
        const auto across = projectPointShadow(direction, radius);
        EXPECT_NE(across.face, face);
        EXPECT_GE(across.u, 0);
        EXPECT_LE(across.u, 1);
        const auto back = pointShadowDirection(across.face, across.u, across.v);
        const float a = std::hypot(direction.x, direction.y, direction.z),
                    b = std::hypot(back.x, back.y, back.z);
        EXPECT_NEAR(direction.x / a, back.x / b, .000001F);
        EXPECT_NEAR(direction.y / a, back.y / b, .000001F);
        EXPECT_NEAR(direction.z / a, back.z / b, .000001F);
      }
    }
}

TEST(InteriorLightState, FrameEnablesRequireExactCountAndBinaryValues) {
  EXPECT_NO_THROW(validatePointLightEnables({}, 0));
  const std::array<std::uint8_t, 8> all{0, 1, 0, 1, 0, 1, 0, 1};
  EXPECT_NO_THROW(validatePointLightEnables(all, 8));
  EXPECT_THROW(validatePointLightEnables(all, 7), std::invalid_argument);
  EXPECT_THROW(validatePointLightEnables({}, 1), std::invalid_argument);
  auto invalid = all;
  invalid[7] = 2;
  EXPECT_THROW(validatePointLightEnables(invalid, 8), std::invalid_argument);
}

TEST(PointShadowProfile,
     RequiresSampledDepthAndBoundedImageLimitsWithD16Fallback) {
  constexpr auto features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  const VkImageFormatProperties image{
      {512, 512, 1}, 1, 24, VK_SAMPLE_COUNT_1_BIT, 24ULL * 512 * 512 * 4};
  std::array<PointShadowFormatSupport, 2> support{
      {{VK_FORMAT_D16_UNORM, features, image},
       {VK_FORMAT_D32_SFLOAT, features, image}}};
  EXPECT_EQ(choosePointShadowFormat(support, 24), VK_FORMAT_D32_SFLOAT);
  support[1].features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
  EXPECT_EQ(choosePointShadowFormat(support, 24), VK_FORMAT_D16_UNORM);
  support[0].image.maxArrayLayers = 6;
  EXPECT_THROW((void)choosePointShadowFormat(support, 24), std::runtime_error);
  EXPECT_EQ(choosePointShadowFormat(support, 6), VK_FORMAT_D16_UNORM);
  support[0].image.maxExtent.width = 511;
  EXPECT_THROW((void)choosePointShadowFormat(support, 1), std::runtime_error);
  support[0].image = image;
  support[0].image.maxResourceSize = 512 * 512 * 2 - 1;
  EXPECT_THROW((void)choosePointShadowFormat(support, 1), std::runtime_error);
  for (auto count : {0U, 2U, 25U, UINT32_MAX})
    EXPECT_THROW((void)choosePointShadowFormat(support, count),
                 std::invalid_argument);
}
