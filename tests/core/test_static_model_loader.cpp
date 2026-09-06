#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/render/scene_assets.hpp"
#include "core/render/static_model_loader.hpp"
#include "core/resources/shader_provider.hpp"
#include "core/world/scene_assets.hpp"
#include "prototype_level_fixture.hpp"

namespace {
struct FixtureOptions {
  bool indexed{};
  bool required_extension{};
  bool external_buffer{};
  bool child_node{};
  bool extra_mesh{};
  bool extra_primitive{};
  bool sparse_position{};
  bool skin{};
  bool animation{};
  bool morph_target{};
  bool missing_normal{};
  bool mismatched_uv_count{};
  bool out_of_range_index{};
  bool empty{};
  bool oversized{};
  bool material{};
  bool diagonal_normals{};
  bool non_finite_position{};
  int primitive_mode{4};
  std::array<float, 3> node_translation{};
  std::array<float, 3> node_scale{1.0F, 1.0F, 1.0F};
  bool node_transform{};
};

struct View {
  std::size_t offset{};
  std::size_t size{};
};

template <typename Value>
void append(std::vector<std::uint8_t>& bytes, const Value& value) {
  const auto* begin = reinterpret_cast<const std::uint8_t*>(&value);
  bytes.insert(bytes.end(), begin, begin + sizeof(Value));
}

void align4(std::vector<std::uint8_t>& bytes, std::uint8_t padding = 0) {
  while (bytes.size() % 4 != 0) {
    bytes.push_back(padding);
  }
}

template <typename Value>
View appendValues(std::vector<std::uint8_t>& bytes,
                  const std::vector<Value>& values) {
  align4(bytes);
  const std::size_t offset = bytes.size();
  for (const Value& value : values) {
    append(bytes, value);
  }
  return {offset, bytes.size() - offset};
}

std::string vec3(const std::array<float, 3>& value) {
  std::ostringstream stream;
  stream << '[' << value[0] << ',' << value[1] << ',' << value[2] << ']';
  return stream.str();
}

std::vector<std::uint8_t> makeGlb(const FixtureOptions& options = {}) {
  using Vec3 = std::array<float, 3>;
  using Vec2 = std::array<float, 2>;
  std::vector<Vec3> positions = {
      {{0.0F, 0.0F, 0.0F}}, {{1.0F, 0.0F, 0.0F}}, {{0.0F, 1.0F, 0.0F}}};
  if (options.non_finite_position) {
    positions[1][0] = std::numeric_limits<float>::infinity();
  }
  const float diagonal = std::sqrt(0.5F);
  const Vec3 normal = options.diagonal_normals ? Vec3{diagonal, diagonal, 0.0F}
                                               : Vec3{0.0F, 0.0F, 1.0F};
  const std::vector<Vec3> normals(3, normal);
  const std::vector<Vec2> uvs = {
      {{0.25F, 0.5F}}, {{1.25F, -0.5F}}, {{0.0F, 2.0F}}};
  const std::vector<std::uint16_t> indices =
      options.out_of_range_index ? std::vector<std::uint16_t>{4, 0, 1}
                                 : std::vector<std::uint16_t>{2, 0, 1};

  std::vector<std::uint8_t> binary;
  std::vector<View> views;
  views.push_back(appendValues(binary, positions));
  views.push_back(appendValues(binary, normals));
  views.push_back(appendValues(binary, uvs));
  if (options.indexed) {
    views.push_back(appendValues(binary, indices));
  }
  std::size_t sparse_indices_view = 0;
  std::size_t sparse_values_view = 0;
  if (options.sparse_position) {
    sparse_indices_view = views.size();
    views.push_back(appendValues(binary, std::vector<std::uint8_t>{0}));
    sparse_values_view = views.size();
    views.push_back(
        appendValues(binary, std::vector<Vec3>{{{0.5F, 0.5F, 0.0F}}}));
  }
  align4(binary);

  const std::uint64_t ordinary_count = options.empty ? 0U : 3U;
  const std::string count =
      options.oversized ? "4294967296" : std::to_string(ordinary_count);

  std::ostringstream primitive;
  primitive << "{\"attributes\":{\"POSITION\":0";
  if (!options.missing_normal) {
    primitive << ",\"NORMAL\":1";
  }
  primitive << ",\"TEXCOORD_0\":2}";
  if (options.indexed) {
    primitive << ",\"indices\":3";
  }
  primitive << ",\"mode\":" << options.primitive_mode;
  if (options.material) {
    primitive << ",\"material\":0";
  }
  if (options.morph_target) {
    primitive << ",\"targets\":[{\"POSITION\":0}]";
  }
  primitive << '}';

  std::ostringstream json;
  json << "{\"asset\":{\"version\":\"2.0\"},\"scene\":0";
  if (options.required_extension) {
    json << ",\"extensionsRequired\":[\"TEST_required\"]";
  }
  json << ",\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0";
  if (options.child_node) {
    json << ",\"children\":[1]";
  }
  if (options.skin) {
    json << ",\"skin\":0";
  }
  if (options.node_transform) {
    json << ",\"translation\":" << vec3(options.node_translation)
         << ",\"scale\":" << vec3(options.node_scale);
  }
  json << '}';
  if (options.child_node) {
    json << ",{}";
  }
  json << "],\"meshes\":[{\"primitives\":[" << primitive.str();
  if (options.extra_primitive) {
    json << ',' << primitive.str();
  }
  json << "]}";
  if (options.extra_mesh) {
    json << ",{\"primitives\":[" << primitive.str() << "]}";
  }
  json << "],\"buffers\":[{\"byteLength\":" << binary.size();
  if (options.external_buffer) {
    json << ",\"uri\":\"external.bin\"";
  }
  json << "}],\"bufferViews\":[";
  for (std::size_t index = 0; index < views.size(); ++index) {
    if (index != 0) {
      json << ',';
    }
    json << "{\"buffer\":0,\"byteOffset\":" << views[index].offset
         << ",\"byteLength\":" << views[index].size << '}';
  }
  json << "],\"accessors\":["
       << "{\"bufferView\":0,\"componentType\":5126,\"count\":" << count
       << ",\"type\":\"VEC3\"";
  if (options.sparse_position) {
    json << ",\"sparse\":{\"count\":1,\"indices\":{\"bufferView\":"
         << sparse_indices_view
         << ",\"componentType\":5121},\"values\":{\"bufferView\":"
         << sparse_values_view << "}}";
  }
  json << "},{\"bufferView\":1,\"componentType\":5126,\"count\":" << count
       << ",\"type\":\"VEC3\"},"
       << "{\"bufferView\":2,\"componentType\":5126,\"count\":"
       << (options.mismatched_uv_count ? "2" : count) << ",\"type\":\"VEC2\"}";
  if (options.indexed) {
    json << ",{\"bufferView\":3,\"componentType\":5123,\"count\":3,"
            "\"type\":\"SCALAR\"}";
  }
  json << ']';
  if (options.skin) {
    json << ",\"skins\":[{\"joints\":[0]}]";
  }
  if (options.animation) {
    json << ",\"animations\":[{}]";
  }
  if (options.material) {
    json << ",\"materials\":[{\"name\":\"ignored\"}]";
  }
  json << '}';

  std::string json_chunk = json.str();
  while (json_chunk.size() % 4 != 0) {
    json_chunk.push_back(' ');
  }
  const std::uint32_t total_length = static_cast<std::uint32_t>(
      12 + 8 + json_chunk.size() + 8 + binary.size());
  std::vector<std::uint8_t> glb;
  append(glb, std::uint32_t{0x46546C67});
  append(glb, std::uint32_t{2});
  append(glb, total_length);
  append(glb, static_cast<std::uint32_t>(json_chunk.size()));
  append(glb, std::uint32_t{0x4E4F534A});
  glb.insert(glb.end(), json_chunk.begin(), json_chunk.end());
  append(glb, static_cast<std::uint32_t>(binary.size()));
  append(glb, std::uint32_t{0x004E4942});
  glb.insert(glb.end(), binary.begin(), binary.end());
  return glb;
}

std::vector<std::uint8_t> replaceGlbJson(std::vector<std::uint8_t> bytes,
                                         std::string_view from,
                                         std::string_view to) {
  std::uint32_t length = 0;
  std::memcpy(&length, bytes.data() + 12, sizeof(length));
  std::string json(bytes.begin() + 20, bytes.begin() + 20 + length);
  const auto found = json.find(from);
  if (found == std::string::npos)
    throw std::runtime_error("Missing fixture JSON text");
  json.replace(found, from.size(), to);
  while (json.size() % 4) json.push_back(' ');
  std::vector<std::uint8_t> result(bytes.begin(), bytes.begin() + 12);
  append(result, static_cast<std::uint32_t>(json.size()));
  append(result, std::uint32_t{0x4E4F534A});
  result.insert(result.end(), json.begin(), json.end());
  result.insert(result.end(), bytes.begin() + 20 + length, bytes.end());
  const auto total = static_cast<std::uint32_t>(result.size());
  std::memcpy(result.data() + 8, &total, sizeof(total));
  return result;
}

class FixtureFile {
 public:
  explicit FixtureFile(const std::vector<std::uint8_t>& bytes) {
    static std::uint32_t sequence = 0;
    path_ = std::filesystem::temp_directory_path() /
            ("near_laugh_static_model_" + std::to_string(++sequence) + ".glb");
    std::ofstream output(path_, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
      throw std::runtime_error("Failed to write GLB fixture");
    }
  }
  ~FixtureFile() { std::filesystem::remove(path_); }

  FixtureFile(const FixtureFile&) = delete;
  FixtureFile& operator=(const FixtureFile&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return path_;
  }

 private:
  std::filesystem::path path_{};
};

PrototypeStaticProp identityPlacement() {
  PrototypeStaticProp placement = loadPackagedPrototypeLevel().props().front();
  placement.translation = {};
  placement.yaw_degrees = 0.0F;
  placement.uniform_scale = 1.0F;
  return placement;
}

void expectFailure(const FixtureOptions& options, const std::string& reason) {
  const FixtureFile fixture(makeGlb(options));
  try {
    static_cast<void>(
        loadStaticModelVertices(fixture.path(), identityPlacement()));
    FAIL() << "Expected static model failure containing: " << reason;
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find(fixture.path().string()), std::string::npos);
    EXPECT_NE(message.find(reason), std::string::npos) << message;
  }
}
}  // namespace

TEST(StaticModelLoader, RejectsMalformedAndUnsupportedStructureWithPath) {
  {
    const FixtureFile fixture({0, 1, 2, 3});
    EXPECT_THROW(static_cast<void>(loadStaticModelVertices(
                     fixture.path(), identityPlacement())),
                 std::runtime_error);
  }
  FixtureOptions options;
  options.required_extension = true;
  expectFailure(options, "required extensions");
  options = {};
  options.external_buffer = true;
  expectFailure(options, "embedded GLB buffer");
  options = {};
  options.child_node = true;
  expectFailure(options, "child nodes");
  options = {};
  options.extra_mesh = true;
  expectFailure(options, "exactly one mesh");
  options = {};
  options.extra_primitive = true;
  expectFailure(options, "exactly one mesh primitive");
  options = {};
  options.primitive_mode = 1;
  expectFailure(options, "triangle-list");
  options = {};
  options.sparse_position = true;
  expectFailure(options, "sparse accessors");
  options = {};
  options.skin = true;
  expectFailure(options, "skins");
  options = {};
  options.animation = true;
  expectFailure(options, "animations");
  options = {};
  options.morph_target = true;
  expectFailure(options, "morph targets");
}

TEST(StaticModelLoader, ExpandsIndexedAndNonIndexedDataInDeclaredOrder) {
  {
    const FixtureFile fixture(makeGlb());
    const auto vertices =
        loadStaticModelVertices(fixture.path(), identityPlacement());
    ASSERT_EQ(vertices.size(), 3U);
    EXPECT_FLOAT_EQ(vertices[0].position[0], 0.0F);
    EXPECT_FLOAT_EQ(vertices[1].position[0], 1.0F);
    EXPECT_FLOAT_EQ(vertices[2].position[1], 1.0F);
  }
  FixtureOptions indexed;
  indexed.indexed = true;

  const FixtureFile fixture(makeGlb(indexed));
  const auto vertices =
      loadStaticModelVertices(fixture.path(), identityPlacement());
  ASSERT_EQ(vertices.size(), 3U);
  EXPECT_FLOAT_EQ(vertices[0].position[1], 1.0F);
  EXPECT_FLOAT_EQ(vertices[1].position[0], 0.0F);
  EXPECT_FLOAT_EQ(vertices[2].position[0], 1.0F);
  for (const PositionColorVertex& vertex : vertices) {
    EXPECT_EQ(vertex.color[0], 255U);
    EXPECT_EQ(vertex.color[1], 255U);
    EXPECT_EQ(vertex.color[2], 255U);
    EXPECT_EQ(vertex.color[3], 255U);
    EXPECT_EQ(vertex.texture_layer, 0U);
  }
}

TEST(StaticModelLoader, RejectsInvalidAccessorDataDeterministically) {
  FixtureOptions options;
  options.missing_normal = true;
  expectFailure(options, "NORMAL");
  options = {};
  options.mismatched_uv_count = true;
  expectFailure(options, "counts do not match");
  options = {};
  options.indexed = true;
  options.out_of_range_index = true;
  expectFailure(options, "validation failed");
  options = {};
  options.empty = true;
  expectFailure(options, "embedded data");
  options = {};
  options.oversized = true;
  expectFailure(options, "too large");
}

TEST(StaticModelLoader, CombinesNodeAndPlacementTransformsAndPreservesUvs) {
  FixtureOptions options;
  options.node_transform = true;
  options.node_translation = {1.0F, 2.0F, 3.0F};
  options.node_scale = {2.0F, 1.0F, 0.5F};
  options.diagonal_normals = true;
  const FixtureFile fixture(makeGlb(options));
  PrototypeStaticProp placement = identityPlacement();
  placement.translation = {10.0F, 1.0F, -4.0F};
  placement.yaw_degrees = 90.0F;
  placement.uniform_scale = 2.0F;
  const auto vertices = loadStaticModelVertices(fixture.path(), placement);
  ASSERT_EQ(vertices.size(), 3U);
  EXPECT_NEAR(vertices[0].position[0], 16.0F, 0.0001F);
  EXPECT_NEAR(vertices[0].position[1], 5.0F, 0.0001F);
  EXPECT_NEAR(vertices[0].position[2], -6.0F, 0.0001F);
  EXPECT_NEAR(vertices[1].position[0], 16.0F, 0.0001F);
  EXPECT_NEAR(vertices[1].position[1], 5.0F, 0.0001F);
  EXPECT_NEAR(vertices[1].position[2], -10.0F, 0.0001F);
  EXPECT_NEAR(vertices[0].normal[0], 0.0F, 0.0001F);
  EXPECT_NEAR(vertices[0].normal[1], 0.894427F, 0.0001F);
  EXPECT_NEAR(vertices[0].normal[2], -0.447214F, 0.0001F);
  EXPECT_FLOAT_EQ(vertices[0].texture_coordinates[0], 0.25F);
  EXPECT_FLOAT_EQ(vertices[0].texture_coordinates[1], 0.5F);
  EXPECT_FLOAT_EQ(vertices[1].texture_coordinates[0], 1.25F);
  EXPECT_FLOAT_EQ(vertices[1].texture_coordinates[1], -0.5F);
}

TEST(StaticModelLoader, RejectsSingularAndNonFiniteGeometry) {
  FixtureOptions options;
  options.node_transform = true;
  options.node_scale = {0.0F, 1.0F, 1.0F};
  expectFailure(options, "singular");
  options = {};
  options.non_finite_position = true;
  expectFailure(options, "non-finite");
}

TEST(StaticModelLoader, LoadsPackagedPrototypeChairThroughProductionPath) {
  const std::filesystem::path path =
      std::filesystem::absolute("resources/models/prototype_chair.glb")
          .lexically_normal();
  const auto vertices = loadStaticModelVertices(
      path, loadPackagedPrototypeLevel().props().front());
  ASSERT_FALSE(vertices.empty());
  EXPECT_EQ(vertices.size() % 3, 0U);
  for (const PositionColorVertex& vertex : vertices) {
    for (float component : vertex.position) {
      EXPECT_TRUE(std::isfinite(component));
    }
    const float normal_length = std::sqrt(vertex.normal[0] * vertex.normal[0] +
                                          vertex.normal[1] * vertex.normal[1] +
                                          vertex.normal[2] * vertex.normal[2]);
    EXPECT_NEAR(normal_length, 1.0F, 0.0001F);
    EXPECT_TRUE(std::isfinite(vertex.texture_coordinates[0]));
    EXPECT_TRUE(std::isfinite(vertex.texture_coordinates[1]));
    EXPECT_EQ(vertex.color[0], 255U);
    EXPECT_EQ(vertex.color[1], 255U);
    EXPECT_EQ(vertex.color[2], 255U);
    EXPECT_EQ(vertex.color[3], 255U);
    EXPECT_EQ(vertex.texture_layer, 0U);
  }
}

TEST(StaticModelLoader, LoadsTheSelectedPreparedMaterialsAndCatalogBounds) {
  const auto root = std::filesystem::absolute("resources");
  for (const auto id : {"apartment-chair", "apartment-table", "apartment-phone",
                        "apartment-radio"}) {
    SCOPED_TRACE(id);
    const auto model = loadStaticModel(sceneModelPath(root, id));
    ASSERT_FALSE(model.vertices.empty());
    EXPECT_EQ(model.vertices.size() % 3, 0U);
    EXPECT_EQ(model.material.image.width, 128U);
    EXPECT_EQ(model.material.image.height, 128U);
    EXPECT_TRUE(model.material.nearest);
    EXPECT_EQ(model.material.base_color_factor,
              (std::array<float, 4>{1, 1, 1, 1}));
    EXPECT_EQ(model.material.alpha_mask,
              std::string_view{id} == "apartment-phone");
    EXPECT_FLOAT_EQ(model.material.alpha_cutoff, 0.5F);
    const auto* catalog = findSceneModel(id);
    ASSERT_NE(catalog, nullptr);
    EXPECT_NEAR(model.minimum.x, catalog->bounds_min.x, 0.000002F);
    EXPECT_NEAR(model.minimum.y, catalog->bounds_min.y, 0.000002F);
    EXPECT_NEAR(model.minimum.z, catalog->bounds_min.z, 0.000002F);
    EXPECT_NEAR(model.maximum.x, catalog->bounds_max.x, 0.000002F);
    EXPECT_NEAR(model.maximum.y, catalog->bounds_max.y, 0.000002F);
    EXPECT_NEAR(model.maximum.z, catalog->bounds_max.z, 0.000002F);
  }
  const auto phone = loadStaticModel(sceneModelPath(root, "apartment-phone"));
  bool covered = false, discarded = false;
  for (std::size_t i = 3; i < phone.material.image.pixels.size(); i += 4) {
    const bool coverage = materialCoversSample(
        phone.material,
        static_cast<float>(phone.material.image.pixels[i]) / 255);
    covered |= coverage;
    discarded |= !coverage;
  }
  EXPECT_TRUE(covered);
  EXPECT_TRUE(discarded);
}

TEST(StaticModelLoader, RejectsUnpreparedShadingAndBlendWithAssetContext) {
  FixtureOptions options;
  options.material = true;
  expectFailure(options, "unsupported shading inputs");

  const auto path =
      sceneModelPath(std::filesystem::absolute("resources"), "apartment-phone");
  const FixtureFile fixture(
      replaceGlbJson(readBinaryFile(path), "\"MASK\"", "\"BLEND\""));
  try {
    static_cast<void>(loadStaticModel(fixture.path()));
    FAIL() << "BLEND must fail the controlled profile";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string{error.what()}.find("BLEND"), std::string::npos);
    EXPECT_NE(std::string{error.what()}.find(fixture.path().string()),
              std::string::npos);
  }
}

TEST(SceneAssets, RepeatedPlacementsShareMaterialAndKeepIndependentTransforms) {
  LevelDocument document;
  auto first = identityPlacement();
  first.id = "chair-a";
  first.model = "apartment-chair";
  auto second = first;
  second.id = "chair-b";
  second.translation = {3, 2, -4};
  second.yaw_degrees = 90;
  second.uniform_scale = 2;
  document.props = {first, second};
  const auto root = std::filesystem::absolute("resources");
  const auto prepared = prepareSceneAssets(root, document);
  const auto model = loadStaticModel(sceneModelPath(root, first.model));
  const auto placed = placeStaticModelVertices(model, second);
  ASSERT_EQ(prepared.materials.size(), 1U);
  ASSERT_EQ(prepared.props.size(), 1U);
  ASSERT_EQ(prepared.props.front().vertices.size(), model.vertices.size() * 2);
  const auto& vertices = prepared.props.front().vertices;
  for (std::size_t i = 0; i < model.vertices.size(); ++i) {
    for (int axis = 0; axis < 3; ++axis) {
      EXPECT_FLOAT_EQ(vertices[i].position[axis],
                      model.vertices[i].position[axis]);
      EXPECT_FLOAT_EQ(vertices[i + model.vertices.size()].position[axis],
                      placed[i].position[axis]);
      EXPECT_FLOAT_EQ(vertices[i + model.vertices.size()].normal[axis],
                      placed[i].normal[axis]);
    }
    for (int axis = 0; axis < 2; ++axis)
      EXPECT_FLOAT_EQ(
          vertices[i + model.vertices.size()].texture_coordinates[axis],
          model.vertices[i].texture_coordinates[axis]);
  }
  EXPECT_TRUE(prepared.world.empty());
}

TEST(SceneAssets, SelectedOnlyPreflightAndEmptySceneNeedNoUnusedModelFiles) {
  const auto absent_root =
      std::filesystem::absolute("missing-assets-for-empty-scene");
  LevelDocument document;
  const auto empty = prepareSceneAssets(absent_root, document);
  EXPECT_TRUE(empty.props.empty());
  EXPECT_TRUE(empty.world.empty());
  ASSERT_EQ(empty.materials.size(), 1U);
  EXPECT_EQ(empty.materials[0].data.image.pixels,
            (std::vector<std::uint8_t>{255, 255, 255, 255}));
  auto prop = identityPlacement();
  prop.id = "required-chair";
  prop.model = "apartment-chair";
  document.props.push_back(prop);
  try {
    validateSceneAssets(document, absent_root);
    FAIL() << "Selected missing model must fail preflight";
  } catch (const std::runtime_error& error) {
    const std::string message{error.what()};
    EXPECT_NE(message.find("required-chair"), std::string::npos);
    EXPECT_NE(message.find("apartment-chair"), std::string::npos);
    EXPECT_NE(message.find("apartment_chair.glb"), std::string::npos);
  }
  document.props.resize(level_maximum_prop_count + 1, prop);
  EXPECT_THROW(static_cast<void>(prepareSceneAssets(absent_root, document)),
               std::runtime_error);
}

TEST(SceneAssets,
     LegacyChairUsesExplicitLegacyMaterialAndDoorAliasStaysOpaque) {
  LevelDocument document;
  document.props.push_back(identityPlacement());
  document.doors.push_back(DoorDefinition{});
  const auto prepared =
      prepareSceneAssets(std::filesystem::absolute("resources"), document);
  ASSERT_EQ(prepared.materials.size(), 1U);
  ASSERT_EQ(prepared.props.size(), 1U);
  ASSERT_TRUE(prepared.obstacle_material.has_value());
  EXPECT_EQ(prepared.props.front().material, *prepared.obstacle_material);
  EXPECT_EQ(prepared.materials[0].id, "prototype-obstacle");
  EXPECT_FALSE(prepared.materials[0].data.nearest);
  EXPECT_FALSE(prepared.materials[0].data.alpha_mask);
}

TEST(StaticModelLoader, RejectsOverflowingBufferAndAccessorRangesBeforeRead) {
  for (const auto& replacement :
       {replaceGlbJson(makeGlb(), "\"byteOffset\":0",
                       "\"byteOffset\":18446744073709551615"),
        replaceGlbJson(makeGlb(), "\"bufferView\":0,\"componentType\"",
                       "\"bufferView\":0,\"byteOffset\":18446744073709551615,"
                       "\"componentType\"")}) {
    const FixtureFile fixture(replacement);
    EXPECT_THROW(static_cast<void>(loadStaticModel(fixture.path())),
                 std::runtime_error);
  }
}

TEST(StaticModelLoader, RejectsExternalImagesUnsupportedSamplersAndShading) {
  const auto bytes = readBinaryFile(sceneModelPath(
      std::filesystem::absolute("resources"), "apartment-phone"));
  const std::array<std::pair<std::string_view, std::string_view>, 5> changes{
      {{"\"mimeType\":\"image/png\"",
        "\"mimeType\":\"image/png\",\"uri\":\"outside.png\""},
       {"\"magFilter\":9728", "\"magFilter\":9729"},
       {"\"metallicFactor\":0", "\"metallicFactor\":1"},
       {"\"alphaCutoff\":0.5", "\"alphaCutoff\":2"},
       {"\"baseColorFactor\":[1,1,1,1]", "\"baseColorFactor\":[1,1,1,2]"}}};
  for (const auto& [from, to] : changes) {
    SCOPED_TRACE(from);
    const FixtureFile fixture(replaceGlbJson(bytes, from, to));
    EXPECT_THROW(static_cast<void>(loadStaticModel(fixture.path())),
                 std::runtime_error);
  }
  auto corrupt_png = bytes;
  const std::array<std::uint8_t, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
  const auto png = std::search(corrupt_png.begin(), corrupt_png.end(),
                               signature.begin(), signature.end());
  ASSERT_NE(png, corrupt_png.end());
  *png = 0;
  const FixtureFile fixture(corrupt_png);
  EXPECT_THROW(static_cast<void>(loadStaticModel(fixture.path())),
               std::runtime_error);
}
