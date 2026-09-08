#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "core/animation/character_animation.hpp"
#include "core/animation/character_catalog.hpp"

namespace {
using Json = nlohmann::json;
using Vec3 = std::array<float, 3>;
using Vec4 = std::array<float, 4>;
constexpr AnimationMatrix identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

struct GlbFixture {
  Json document;
  std::vector<std::uint8_t> binary;
  GlbFixture() {
    document = {
      {"asset", {{"version", "2.0"}}}, {"scene", 0},
      {"scenes", {{{"nodes", {0, 2}}}}},
      {"nodes", {{{"translation", {1,0,0}}, {"children", {1}}},
                  {{"translation", {0,1,0}}},
                  {{"mesh", 0}, {"skin", 0}}}},
      {"skins", {{{"joints", {0,1}}, {"skeleton", 0}, {"inverseBindMatrices", 6}}}},
      {"materials", {{{"doubleSided", true}, {"alphaMode", "OPAQUE"},
                      {"pbrMetallicRoughness", {{"baseColorFactor", {0.2,0.4,0.6,1}},
                       {"metallicFactor", 0}, {"roughnessFactor", 1}}}}}},
      {"meshes", {{{"primitives", {{{"attributes",
          {{"POSITION",0}, {"NORMAL",1}, {"TEXCOORD_0",2}, {"JOINTS_0",3}, {"WEIGHTS_0",4}}},
          {"indices",5}, {"material",0}, {"mode",4}}}}}}}
    };
    add<Vec3>({{2,1,0}, {1,2,0}, {1,1,0}}, "VEC3", 5126);
    add<Vec3>({{1,0,0}, {1,0,0}, {1,0,0}}, "VEC3", 5126);
    add<std::array<float,2>>({{0,0}, {1,0}, {0,1}}, "VEC2", 5126);
    add<std::array<std::uint8_t,4>>({{1,0,0,0}, {0,1,0,0}, {0,0,0,0}}, "VEC4", 5121);
    add<Vec4>({{1,0,0,0}, {0.5F,0.5F,0,0}, {1,0,0,0}}, "VEC4", 5126);
    add<std::uint16_t>({0,1,2}, "SCALAR", 5123);
    auto root_inverse = identity;
    root_inverse[12] = -1;
    auto child_inverse = root_inverse;
    child_inverse[13] = -1;
    add<AnimationMatrix>({root_inverse, child_inverse}, "MAT4", 5126);
    add<float>({0,1}, "SCALAR", 5126);
    const float q = std::sqrt(0.5F);
    // The second quaternion's sign deliberately requires shortest-arc sampling.
    add<Vec4>({{0,0,0,1}, {0,0,-q,-q}}, "VEC4", 5126);
    add<Vec3>({{0,1,0}, {0,2,0}}, "VEC3", 5126);
    for (const char* name : {"idle", "walk", "interact"}) {
      Json clip = {{"name",name},
                   {"samplers", {{{"input",7}, {"output",8}, {"interpolation","LINEAR"}}}},
                   {"channels", {{{"sampler",0}, {"target",{{"node",1}, {"path","rotation"}}}}}}};
      if (std::string_view(name) == "interact") {
        clip["samplers"].push_back({{"input",7}, {"output",9}, {"interpolation","LINEAR"}});
        clip["channels"].push_back({{"sampler",1}, {"target",{{"node",1}, {"path","translation"}}}});
      }
      document["animations"].push_back(std::move(clip));
    }
  }
  template <typename T>
  void add(const std::vector<T>& values, const char* type, int component) {
    while (binary.size() % 4) binary.push_back(0);
    const auto offset = binary.size();
    const auto* begin = reinterpret_cast<const std::uint8_t*>(values.data());
    binary.insert(binary.end(), begin, begin + values.size() * sizeof(T));
    const auto index = document["bufferViews"].size();
    document["bufferViews"].push_back(
        {{"buffer",0}, {"byteOffset",offset}, {"byteLength",values.size()*sizeof(T)}});
    document["accessors"].push_back(
        {{"bufferView",index}, {"componentType",component}, {"count",values.size()}, {"type",type}});
  }
  template <typename T>
  void overwrite(std::size_t accessor, std::size_t byte_offset, T value) {
    const auto view = document["accessors"][accessor]["bufferView"].get<std::size_t>();
    const auto offset = document["bufferViews"][view]["byteOffset"].get<std::size_t>();
    std::memcpy(binary.data() + offset + byte_offset, &value, sizeof(T));
  }
  void write(const std::filesystem::path& path) {
    while (binary.size() % 4) binary.push_back(0);
    document["buffers"][0]["byteLength"] = binary.size();
    std::string json = document.dump();
    while (json.size() % 4) json.push_back(' ');
    std::ofstream file(path, std::ios::binary);
    const auto integer = [&](std::uint32_t value) {
      file.write(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    integer(0x46546c67); integer(2);
    integer(static_cast<std::uint32_t>(28 + json.size() + binary.size()));
    integer(static_cast<std::uint32_t>(json.size())); integer(0x4e4f534a);
    file.write(json.data(), static_cast<std::streamsize>(json.size()));
    integer(static_cast<std::uint32_t>(binary.size())); integer(0x004e4942);
    file.write(reinterpret_cast<const char*>(binary.data()),
               static_cast<std::streamsize>(binary.size()));
  }
};
struct TempGlb {
  std::filesystem::path path;
  TempGlb() : path(std::filesystem::temp_directory_path() /
      ("near-laugh-character-" + std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) + ".glb")) {}
  ~TempGlb() { std::error_code error; std::filesystem::remove(path, error); }
};
std::shared_ptr<const CharacterAsset> tiny() {
  TempGlb temporary;
  GlbFixture fixture;
  fixture.write(temporary.path);
  return loadCharacterAsset(temporary.path);
}
void expectPoseNear(const CharacterLocalPose& a, const CharacterLocalPose& b,
                    float tolerance = 0.000002F) {
  ASSERT_EQ(a.size(), b.size());
  for (std::size_t n = 0; n < a.size(); ++n) {
    for (std::size_t axis = 0; axis < 3; ++axis)
      EXPECT_NEAR(a[n].translation[axis], b[n].translation[axis], tolerance);
    float dot = 0;
    for (std::size_t axis = 0; axis < 4; ++axis)
      dot += a[n].rotation[axis] * b[n].rotation[axis];
    EXPECT_NEAR(std::abs(dot), 1, tolerance);
  }
}
}

TEST(CharacterAnimation, TinyBindGeometryAndAbsentChannels) {
  const auto asset = tiny();
  ASSERT_EQ(asset->joint_nodes.size(), 2U);
  ASSERT_EQ(asset->vertices.size(), 3U);
  ASSERT_EQ(asset->clips.size(), 3U);
  EXPECT_NE(asset->skeleton_identity, 0U);
  const auto rest = characterRestPose(*asset);
  const auto vertices = deformCharacter(*asset, evaluateCharacterPose(*asset, rest));
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    EXPECT_EQ(vertices[i].position, asset->vertices[i].position);
    EXPECT_EQ(vertices[i].normal, asset->vertices[i].normal);
    EXPECT_EQ(vertices[i].uv, asset->vertices[i].uv);
  }
  const auto sampled = sampleCharacterPose(*asset, "walk", 0.5);
  EXPECT_EQ(sampled[0].translation, rest[0].translation);
  EXPECT_EQ(sampled[0].rotation, rest[0].rotation);
  EXPECT_EQ(sampled[1].translation, rest[1].translation);
}

TEST(CharacterAnimation, TwoJointReferenceSkinningAndWorldYaw) {
  const auto asset = tiny();
  const float diagonal = std::sqrt(0.5F);
  const auto pose = evaluateCharacterPose(*asset, sampleCharacterPose(*asset, "walk", 0.5));
  const auto vertices = deformCharacter(*asset, pose);
  EXPECT_NEAR(vertices[0].position[0], 1 + diagonal, 0.000002);
  EXPECT_NEAR(vertices[0].position[1], 1 + diagonal, 0.000002);
  EXPECT_NEAR(vertices[0].normal[0], diagonal, 0.000002);
  EXPECT_NEAR(vertices[0].normal[1], diagonal, 0.000002);
  // Vertex 1 blends independent root and rotating child contributions.
  EXPECT_NEAR(vertices[1].position[0], 1 - diagonal * 0.5F, 0.000002);
  EXPECT_NEAR(vertices[1].position[1], 1.5F + diagonal * 0.5F, 0.000002);
  const auto placed = deformCharacter(*asset, pose, {{10,3,4},90});
  EXPECT_NEAR(placed[0].position[0], 10, 0.000002);
  EXPECT_NEAR(placed[0].position[1], 4 + diagonal, 0.000002);
  EXPECT_NEAR(placed[0].position[2], 3 - diagonal, 0.000002);
  EXPECT_NEAR(placed[0].normal[0], 0, 0.000002);
  EXPECT_NEAR(placed[0].normal[1], diagonal, 0.000002);
  EXPECT_NEAR(placed[0].normal[2], -diagonal, 0.000002);
  const float length = std::sqrt(placed[1].normal[0] * placed[1].normal[0] +
                                 placed[1].normal[1] * placed[1].normal[1] +
                                 placed[1].normal[2] * placed[1].normal[2]);
  EXPECT_NEAR(length, 1, 0.000002);
}

TEST(CharacterAnimation, LoopEdgesClampAndOneShotCompletion) {
  const auto asset = tiny();
  const auto start = sampleCharacterPose(*asset, "walk", 0);
  expectPoseNear(start, sampleCharacterPose(*asset, "walk", 1));
  expectPoseNear(start, sampleCharacterPose(*asset, "walk", 200));
  expectPoseNear(sampleCharacterPose(*asset, "walk", 0.25),
                 sampleCharacterPose(*asset, "walk", 200.25));
  const auto final = sampleCharacterPose(*asset, "interact", 1);
  expectPoseNear(final, sampleCharacterPose(*asset, "interact", 50));
  EXPECT_FLOAT_EQ(final[1].translation[1], 2);
  CharacterPlayback playback(asset, "interact");
  EXPECT_FALSE(playback.advance(0.5));
  EXPECT_TRUE(playback.advance(0.5));
  EXPECT_FALSE(playback.advance(1));
  EXPECT_FALSE(playback.advance(100));
  expectPoseNear(playback.localPose(), final);
  playback.restart();
  EXPECT_TRUE(playback.advance(10));
  EXPECT_FALSE(playback.advance(10));
}

TEST(CharacterAnimation, PauseSeekAndIndependentInstances) {
  const auto asset = tiny();
  CharacterPlayback a(asset), b(asset);
  a.selectClip("interact");
  a.advance(0.05);
  a.setPaused(true);
  const auto frozen = a.localPose();
  EXPECT_FALSE(a.advance(20));
  expectPoseNear(a.localPose(), frozen);
  EXPECT_DOUBLE_EQ(a.time(), 0.05);
  a.seek(0.7);
  expectPoseNear(a.localPose(), sampleCharacterPose(*asset, "interact", 0.7));
  EXPECT_FALSE(a.advance(20));
  a.setPaused(false);
  EXPECT_TRUE(a.advance(0.3));
  a.seek(1);
  EXPECT_FALSE(a.advance(1));
  a.seek(0);
  EXPECT_TRUE(a.advance(1));
  EXPECT_DOUBLE_EQ(b.time(), 0);
  EXPECT_EQ(b.clip(), "idle");
}

TEST(CharacterAnimation, InterruptedTransitionCapturesDisplayedLocalPose) {
  const auto asset = tiny();
  CharacterPlayback playback(asset);
  playback.seek(0.7);
  const auto initial = playback.localPose();
  playback.selectClip("walk");
  expectPoseNear(playback.localPose(), initial);
  playback.advance(0.05);
  const auto interrupted = playback.localPose();
  playback.selectClip("interact");
  expectPoseNear(playback.localPose(), interrupted);
  for (std::size_t node = 0; node < interrupted.size(); ++node) {
    EXPECT_EQ(playback.localPose()[node].rotation, interrupted[node].rotation);
    EXPECT_EQ(playback.localPose()[node].translation, interrupted[node].translation);
  }
  playback.advance(0.075);
  const auto midway = playback.localPose();
  EXPECT_NEAR(midway[1].translation[1], 1 + 0.075 * 0.5, 0.000002);
  playback.advance(0.075);
  expectPoseNear(playback.localPose(), sampleCharacterPose(*asset, "interact", 0.15));
}

TEST(CharacterAnimation, DistanceDrivePreservesInterruptedBlendAndSavedPhase) {
  const auto asset = tiny();
  CharacterPlayback playback(asset);
  playback.seek(.7);
  const auto idle = playback.localPose();
  playback.selectClip("walk");
  playback.drive(.4, 0);
  expectPoseNear(playback.localPose(), idle);
  playback.drive(.4, .075);
  const auto walking = playback.localPose();
  playback.selectClip("idle");
  playback.drive(0, 0);
  expectPoseNear(playback.localPose(), walking);
  playback.drive(.1, .15);
  const auto stopped = playback.localPose();
  playback.selectClip("walk");
  playback.drive(.4, 0);
  expectPoseNear(playback.localPose(), stopped);
  playback.drive(.4, .15);
  expectPoseNear(playback.localPose(), sampleCharacterPose(*asset, "walk", .4));
  EXPECT_DOUBLE_EQ(playback.time(), .4);
}

TEST(CharacterAnimation, EquivalentElapsedTimeAndCommandsAcrossBatching) {
  const auto asset = tiny();
  CharacterPlayback a(asset), b(asset);
  a.advance(0.65);
  for (int i = 0; i < 65; ++i) b.advance(0.01);
  expectPoseNear(a.localPose(), b.localPose());
  a.selectClip("walk"); b.selectClip("walk");
  a.advance(0.06);
  for (int i = 0; i < 6; ++i) b.advance(0.01);
  a.selectClip("interact"); b.selectClip("interact");
  a.advance(0.12);
  for (int i = 0; i < 12; ++i) b.advance(0.01);
  expectPoseNear(a.localPose(), b.localPose());
  EXPECT_TRUE(a.advance(1));
  int completions = 0;
  for (int i = 0; i < 100; ++i) completions += b.advance(0.01) ? 1 : 0;
  EXPECT_EQ(completions, 1);
  expectPoseNear(a.localPose(), b.localPose());
}

TEST(CharacterAnimation, RejectsInvalidPlaybackAndPoseWithoutChangingState) {
  const auto asset = tiny();
  CharacterPlayback playback(asset);
  playback.advance(0.3);
  const auto previous = playback.localPose();
  EXPECT_THROW(playback.selectClip("missing"), std::runtime_error);
  EXPECT_THROW(playback.seek(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
  EXPECT_THROW(playback.seek(-1), std::invalid_argument);
  EXPECT_THROW(playback.advance(std::numeric_limits<double>::infinity()), std::invalid_argument);
  EXPECT_THROW(playback.advance(-1), std::invalid_argument);
  expectPoseNear(playback.localPose(), previous);
  auto pose = playback.pose();
  pose.pop_back();
  EXPECT_THROW((void)deformCharacter(*asset, pose), std::runtime_error);
  pose = playback.pose();
  pose[0][0] = std::numeric_limits<float>::quiet_NaN();
  EXPECT_THROW((void)deformCharacter(*asset, pose), std::runtime_error);
  pose = playback.pose();
  pose[0][0] = 2;
  EXPECT_THROW((void)deformCharacter(*asset, pose), std::runtime_error);
  EXPECT_THROW((void)deformCharacter(*asset, playback.pose(),
      {{0,0,0}, std::numeric_limits<float>::infinity()}), std::runtime_error);
}

TEST(CharacterAnimation, RejectsMalformedAssetsWithFileAndFieldContext) {
  struct Case { const char* name; const char* field; std::function<void(GlbFixture&)> mutate; };
  const std::vector<Case> cases{
    {"missing_normal", "primitive", [](auto& f) { f.document["meshes"][0]["primitives"][0]["attributes"].erase("NORMAL"); }},
    {"missing_skin", "skin", [](auto& f) { f.document["skins"].clear(); }},
    {"cyclic", "hierarchy", [](auto& f) { f.document["nodes"][1]["children"] = {0}; }},
    {"unreachable", "hierarchy", [](auto& f) { f.document["nodes"].push_back(Json::object()); }},
    {"duplicate_child", "hierarchy", [](auto& f) { f.document["nodes"][0]["children"] = {1,1}; }},
    {"bad_index", "indices", [](auto& f) { f.template overwrite<std::uint16_t>(5, 0, 3); }},
    {"bad_joint_zero_weight", "JOINTS", [](auto& f) { f.template overwrite<std::uint8_t>(3, 3, 2); }},
    {"negative_weight", "WEIGHTS", [](auto& f) { f.template overwrite<float>(4, 0, -1); }},
    {"zero_weight", "WEIGHTS", [](auto& f) { f.template overwrite<float>(4, 0, 0); }},
    {"unnormalized_weight", "WEIGHTS", [](auto& f) { f.template overwrite<float>(4, 0, 0.5F); }},
    {"nonfinite_weight", "WEIGHTS", [](auto& f) { f.template overwrite<float>(4, 0, std::numeric_limits<float>::infinity()); }},
    {"nonfinite_position", "POSITION", [](auto& f) { f.template overwrite<float>(0, 0, std::numeric_limits<float>::quiet_NaN()); }},
    {"zero_normal", "NORMAL", [](auto& f) { f.template overwrite<float>(1, 0, 0); }},
    {"nonfinite_uv", "TEXCOORD", [](auto& f) { f.template overwrite<float>(2, 0, std::numeric_limits<float>::infinity()); }},
    {"singular_bind", "inverseBind", [](auto& f) { f.template overwrite<float>(6, 0, 0); }},
    {"inconsistent_bind", "inverseBind", [](auto& f) { f.template overwrite<float>(6, 12 * sizeof(float), -2); }},
    {"nonfinite_bind", "inverseBind", [](auto& f) { f.template overwrite<float>(6, 0, std::numeric_limits<float>::infinity()); }},
    {"duplicate_times", "input", [](auto& f) { f.template overwrite<float>(7, sizeof(float), 0); }},
    {"negative_time", "input", [](auto& f) { f.template overwrite<float>(7, 0, -1); }},
    {"nonfinite_time", "input", [](auto& f) { f.template overwrite<float>(7, 0, std::numeric_limits<float>::quiet_NaN()); }},
    {"duration", "input", [](auto& f) { f.template overwrite<float>(7, sizeof(float), 11); }},
    {"nonunit_rotation", "rotation", [](auto& f) { f.template overwrite<float>(8, 3 * sizeof(float), 0.5F); }},
    {"missing_target", "channel", [](auto& f) { f.document["animations"][0]["channels"][0]["target"].erase("node"); }},
    {"duplicate_channel", "duplicated", [](auto& f) { f.document["animations"][0]["channels"].push_back(f.document["animations"][0]["channels"][0]); }},
    {"scale_channel", "scale", [](auto& f) { f.document["animations"][0]["channels"][0]["target"]["path"] = "scale"; }},
    {"root_animation", "root", [](auto& f) { f.document["animations"][0]["channels"][0]["target"]["node"] = 0; }},
    {"cubic", "LINEAR", [](auto& f) { f.document["animations"][0]["samplers"][0]["interpolation"] = "CUBICSPLINE"; }},
    {"unused_cubic", "unused", [](auto& f) { auto s = f.document["animations"][0]["samplers"][0]; s["interpolation"] = "CUBICSPLINE"; f.document["animations"][0]["samplers"].push_back(s); }},
    {"clip_identity", "names", [](auto& f) { f.document["animations"][0]["name"] = "combat"; }},
    {"duplicate_clip", "names", [](auto& f) { f.document["animations"][0]["name"] = "walk"; }},
    {"overflow_offset", "accessor", [](auto& f) { f.document["accessors"][0]["byteOffset"] = 18446744073709551612ULL; }},
    {"overflow_count", "accessor", [](auto& f) { f.document["accessors"][0]["count"] = 18446744073709551615ULL; }},
    {"overflow_view", "bufferView", [](auto& f) { f.document["bufferViews"][0]["byteLength"] = 18446744073709551615ULL; }},
    {"nonunit_scale", "scale", [](auto& f) { f.document["nodes"][1]["scale"] = {1,2,1}; }},
    {"mesh_transform", "identity", [](auto& f) { f.document["nodes"][2]["translation"] = {1,0,0}; }},
    {"texture", "textures", [](auto& f) { f.document["images"] = {{{"uri","external.png"}}}; }},
    {"material_metallic", "material", [](auto& f) { f.document["materials"][0]["pbrMetallicRoughness"]["metallicFactor"] = 1; }},
    {"material_sides", "two-sided", [](auto& f) { f.document["materials"][0]["doubleSided"] = false; }},
    {"extension", "extensions", [](auto& f) { f.document["extensionsUsed"] = {"unsupported"}; }},
    {"deep_extras", "nesting", [](auto& f) {
       Json extra = Json::object();
       for (int i = 0; i < 40; ++i) extra = Json{{"nested", extra}};
       f.document["extras"] = std::move(extra);
     }},
    {"node_count", "80", [](auto& f) { while (f.document["nodes"].size() < 81) f.document["nodes"].push_back(Json::object()); }},
    {"joint_count", "65", [](auto& f) { while (f.document["skins"][0]["joints"].size() < 66) f.document["skins"][0]["joints"].push_back(0); }},
    {"external_buffer", "embedded", [](auto& f) { f.document["buffers"][0]["uri"] = "missing.bin"; }},
    {"unused_sparse", "sparse", [](auto& f) {
       auto a = f.document["accessors"][0];
       a["sparse"] = {{"count",1}, {"indices",{{"bufferView",5}, {"componentType",5123}}},
                      {"values",{{"bufferView",0}}}};
       f.document["accessors"].push_back(a);
     }},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.name);
    GlbFixture fixture;
    test.mutate(fixture);
    TempGlb temporary;
    fixture.write(temporary.path);
    try {
      (void)loadCharacterAsset(temporary.path);
      FAIL() << "Malformed fixture accepted";
    } catch (const std::runtime_error& error) {
      const std::string message = error.what();
      EXPECT_NE(message.find(temporary.path.filename().string()), std::string::npos);
      EXPECT_NE(message.find(test.field), std::string::npos) << message;
    }
  }
}

TEST(CharacterAnimation, RejectsFileAndKeyCountLimitsBeforeHandoff) {
  TempGlb temporary;
  {
    std::ofstream oversized(temporary.path, std::ios::binary);
    oversized.seekp(16U * 1024U * 1024U);
    oversized.put('\0');
  }
  EXPECT_THROW((void)loadCharacterAsset(temporary.path), std::runtime_error);
  GlbFixture fixture;
  const auto time_view = fixture.document["accessors"][7]["bufferView"].get<std::size_t>();
  const auto value_view = fixture.document["accessors"][8]["bufferView"].get<std::size_t>();
  const auto time_offset = fixture.binary.size();
  fixture.binary.resize(time_offset + 257 * sizeof(float) + 257 * sizeof(Vec4));
  fixture.document["bufferViews"][time_view]["byteOffset"] = time_offset;
  fixture.document["bufferViews"][time_view]["byteLength"] = 257 * sizeof(float);
  fixture.document["accessors"][7]["count"] = 257;
  fixture.document["bufferViews"][value_view]["byteOffset"] = time_offset + 257 * sizeof(float);
  fixture.document["bufferViews"][value_view]["byteLength"] = 257 * sizeof(Vec4);
  fixture.document["accessors"][8]["count"] = 257;
  fixture.write(temporary.path);
  try {
    (void)loadCharacterAsset(temporary.path);
    FAIL() << "257-key channel accepted";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("256"), std::string::npos);
  }
}

TEST(CharacterAnimation, PreparedMannequinBindAndEveryClipProduceFiniteGeometry) {
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
                    "resources/characters/test-mannequin.glb";
  const auto asset = loadCharacterAsset(path);
  ASSERT_EQ(asset->joint_nodes.size(), 65U);
  ASSERT_EQ(asset->primitives.size(), 2U);
  ASSERT_EQ(asset->vertices.size(), 8546U);
  EXPECT_EQ(asset->indices.size(), 41232U);
  const auto bind = deformCharacter(*asset, evaluateCharacterPose(*asset, characterRestPose(*asset)));
  for (std::size_t v = 0; v < bind.size(); ++v)
    for (std::size_t axis = 0; axis < 3; ++axis)
      ASSERT_NEAR(bind[v].position[axis], asset->vertices[v].position[axis], 0.0001F);
  EXPECT_NE(asset->primitives[0].base_color, asset->primitives[1].base_color);
  for (const auto& clip : asset->clips) {
    for (int step = 0; step <= 20; ++step) {
      const auto local = sampleCharacterPose(*asset, clip.id, clip.duration * step / 20);
      const auto pose = evaluateCharacterPose(*asset, local);
      const auto vertices = deformCharacter(*asset, pose, {{3,2,-4},57});
      for (const auto& vertex : vertices) {
        for (const float component : vertex.position) ASSERT_TRUE(std::isfinite(component));
        float length_squared = 0;
        for (const float component : vertex.normal) length_squared += component * component;
        ASSERT_NEAR(length_squared, 1, 0.00001F);
      }
    }
  }
}

TEST(CharacterAnimation, GeometryAndParserAllocationLimitsRejectBeforeDecoding) {
  for (const bool vertices : {true, false}) {
    GlbFixture fixture;
    const std::size_t count = vertices ? 10001 : 50001;
    const std::vector<std::size_t> accessors = vertices
        ? std::vector<std::size_t>{0,1,2,3,4} : std::vector<std::size_t>{5};
    for (const auto index : accessors) {
      const auto old_count = fixture.document["accessors"][index]["count"].get<std::size_t>();
      const auto view = fixture.document["accessors"][index]["bufferView"].get<std::size_t>();
      const auto stride = fixture.document["bufferViews"][view]["byteLength"].get<std::size_t>() / old_count;
      while (fixture.binary.size() % 4) fixture.binary.push_back(0);
      fixture.document["bufferViews"][view]["byteOffset"] = fixture.binary.size();
      fixture.document["bufferViews"][view]["byteLength"] = count * stride;
      fixture.document["accessors"][index]["count"] = count;
      fixture.binary.resize(fixture.binary.size() + count * stride);
    }
    TempGlb temporary;
    fixture.write(temporary.path);
    try {
      (void)loadCharacterAsset(temporary.path);
      FAIL() << "Geometry cap accepted";
    } catch (const std::runtime_error& error) {
      EXPECT_NE(std::string(error.what()).find(vertices ? "10000" : "50000"), std::string::npos);
    }
  }
  GlbFixture parser_fixture;
  parser_fixture.document["nodes"] = Json::array();
  for (int i = 0; i < 150000; ++i) parser_fixture.document["nodes"].push_back(Json::object());
  TempGlb temporary;
  parser_fixture.write(temporary.path);
  ASSERT_LT(std::filesystem::file_size(temporary.path), 16U * 1024U * 1024U);
  try {
    (void)loadCharacterAsset(temporary.path);
    FAIL() << "Unbounded cgltf parse allocation accepted";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("allocation budget"), std::string::npos);
  }
}

TEST(CharacterAnimation, NormalizedUnsignedUvAndWeightsAreDecoded) {
  GlbFixture fixture;
  const auto normalized = [&](std::size_t accessor, auto values, const char* type) {
    fixture.add(values, type, 5121);
    auto replacement = fixture.document["accessors"].back();
    replacement["normalized"] = true;
    fixture.document["accessors"][accessor] = replacement;
    fixture.document["accessors"].erase(fixture.document["accessors"].end() - 1);
  };
  normalized(2, std::vector<std::array<std::uint8_t,2>>{{0,0},{255,0},{0,255}}, "VEC2");
  normalized(4, std::vector<std::array<std::uint8_t,4>>{{255,0,0,0},{128,127,0,0},{255,0,0,0}}, "VEC4");
  TempGlb temporary;
  fixture.write(temporary.path);
  const auto asset = loadCharacterAsset(temporary.path);
  EXPECT_FLOAT_EQ(asset->vertices[1].uv[0], 1);
  EXPECT_NEAR(asset->vertices[1].weights[0], 128.0F / 255, 0.000001F);
  EXPECT_NEAR(asset->vertices[1].weights[1], 127.0F / 255, 0.000001F);
}

TEST(CharacterAnimation, RepeatedSmallBatchesReachExactOneShotAndLoopEdges) {
  const auto asset = tiny();
  CharacterPlayback loop(asset, "walk"), once(asset, "interact");
  int completions = 0;
  for (int step = 0; step < 10; ++step) {
    loop.advance(0.1);
    completions += once.advance(0.1) ? 1 : 0;
  }
  EXPECT_EQ(completions, 1);
  EXPECT_DOUBLE_EQ(loop.time(), 0);
  EXPECT_DOUBLE_EQ(once.time(), 1);
  EXPECT_FALSE(once.advance(0.1));
  loop.advance(1);
  EXPECT_DOUBLE_EQ(loop.time(), 0);
}

TEST(CharacterAnimation, CatalogRejectsInvalidMovementContactAndBoundsMetadata) {
  EXPECT_TRUE(characterCatalogIsValid(test_mannequin_catalog));
  auto entry = test_mannequin_catalog;
  entry.walk_cycle_distance_m = 0;
  EXPECT_FALSE(characterCatalogIsValid(entry));
  entry = test_mannequin_catalog;
  entry.walk_cycle_distance_m = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(characterCatalogIsValid(entry));
  entry = test_mannequin_catalog;
  entry.walk_contact_phases = {0.5,0.4};
  EXPECT_FALSE(characterCatalogIsValid(entry));
  entry.walk_contact_phases = {0,0.01};
  EXPECT_FALSE(characterCatalogIsValid(entry));
  entry = test_mannequin_catalog;
  entry.interaction_phase = 1;
  EXPECT_FALSE(characterCatalogIsValid(entry));
  entry = test_mannequin_catalog;
  entry.preview_max[0] = entry.preview_min[0];
  EXPECT_FALSE(characterCatalogIsValid(entry));
  entry = test_mannequin_catalog;
  entry.capsule_radius_m = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(characterCatalogIsValid(entry));
}
