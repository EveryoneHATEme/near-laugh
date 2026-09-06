#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>

#include "core/physics/physics_world.hpp"
#include "core/player/player_controller.hpp"
#include "core/render/prototype_scene.hpp"
#include "core/world/light_switch.hpp"
#include "editor/editor_document.hpp"
#include "editor/editor_picking.hpp"
#include "launcher/launch_options.hpp"

namespace {
LevelDocument interior() {
  EditorDocument editor;
  editor.requestNewInterior();
  return *editor.document();
}
LevelDocument floors() {
  auto level = interior();
  level.solids.push_back({{0, 2.75F, 0},
                          {5, 0.25F, 5},
                          {150, 150, 150, 255},
                          PrototypeSolidKind::Floor,
                          PrototypeSurface::Floor});
  level.entries = {{"lower", {{0, 0, 0}, 90}}, {"upper", {{0, 3, 0}, 180}}};
  level.default_entry = "lower";
  return level;
}
std::string bytes(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
void write(const std::filesystem::path& p, std::string_view text) {
  std::ofstream f(p, std::ios::binary);
  f << text;
}
class InteriorLevel : public testing::Test {
 protected:
  void SetUp() override {
    root = std::filesystem::temp_directory_path() /
           ("near_laugh_interior_" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
  }
  void TearDown() override { std::filesystem::remove_all(root); }
  std::filesystem::path root;
};
}  // namespace

TEST_F(InteriorLevel, VersionFourRoundTripsBothTerrainStatesAndOrderedEntries) {
  for (bool terrain : {false, true}) {
    auto doc = floors();
    if (terrain) {
      doc.terrain = PrototypeTerrain{{-24, -2, -24}, 0.5F, {}};
    }
    const auto p = root / "level.json";
    ASSERT_TRUE(saveLevelDocument(p, doc));
    const auto first = bytes(p);
    const auto loaded = loadLevelDocument(p);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.source_version, 4U);
    EXPECT_EQ(*loaded.document, doc);
    EXPECT_EQ(loaded.document->entries[0].id, "lower");
    ASSERT_TRUE(saveLevelDocument(p, *loaded.document));
    EXPECT_EQ(bytes(p), first);
    EXPECT_EQ(first.back(), '\n');
  }
}

TEST_F(InteriorLevel, LegacyVersionsNormalizeOnlyOnExplicitSave) {
  for (int version : {2, 3}) {
    auto legacy = bytes("tests/fixtures/levels/prototype-v3.level.json");
    if (version == 2) {
      legacy.replace(legacy.find("\"version\": 3"), 12, "\"version\": 2");
      legacy.erase(legacy.find(",\n  \"light_switch\""));
      legacy += "\n}\n";
    }
    auto p = root / "legacy.json";
    write(p, legacy);
    EditorDocument editor;
    ASSERT_TRUE(editor.open(p));
    EXPECT_TRUE(editor.valid());
    EXPECT_FALSE(editor.dirty());
    EXPECT_EQ(editor.sourceVersion(), version);
    EXPECT_EQ(editor.document()->entries.size(), 1U);
    EXPECT_EQ(editor.document()->entries[0].id, "default");
    EXPECT_EQ(editor.document()->default_entry, "default");
    EXPECT_EQ(bytes(p), legacy);
    ASSERT_TRUE(editor.save());
    EXPECT_EQ(loadLevelDocument(p).source_version, 4U);
  }
}

TEST_F(InteriorLevel, AllEntryIdentifiersAndPosesGateSaveAndRuntime) {
  const auto valid = floors();
  const auto reject = [&](const LevelDocument& doc) {
    const auto diagnostics = validateLevelDocument(doc);
    EXPECT_FALSE(diagnostics.empty());
    EXPECT_THROW(static_cast<void>(makePrototypeLevel(doc)),
                 std::invalid_argument);
    EXPECT_FALSE(saveLevelDocument(root / "invalid.json", doc));
  };
  for (const auto& id :
       {std::string{}, std::string{"Upper"}, std::string{"a_b"},
        std::string{"2entry"}, std::string{"a b"}, std::string(65, 'a'),
        std::string{"lower"}}) {
    auto doc = valid;
    doc.entries[1].id = id;
    reject(doc);
  }
  auto doc = valid;
  doc.entries[1].pose.yaw_degrees = std::numeric_limits<float>::infinity();
  reject(doc);
  doc = valid;
  doc.entries[1].pose.foot_position.x = std::numeric_limits<float>::quiet_NaN();
  reject(doc);
  doc = valid;
  doc.default_entry = "missing";
  reject(doc);
  doc = valid;
  doc.entries.clear();
  reject(doc);
  doc = valid;
  for (int i = 2; i < 16; ++i)
    doc.entries.push_back({"entry-" + std::to_string(i), doc.entries[0].pose});
  EXPECT_TRUE(validateLevelDocument(doc).empty());
  doc.entries.push_back({"seventeen", doc.entries[0].pose});
  reject(doc);
  doc = valid;
  doc.entries[1].id = std::string(64, 'a');
  EXPECT_TRUE(validateLevelDocument(doc).empty());
}

TEST_F(InteriorLevel,
       HeightSpecificSupportAndStandingClearanceProtectEveryFloor) {
  const auto valid = floors();
  ASSERT_TRUE(validateLevelDocument(valid).empty());
  auto level = makePrototypeLevel(valid);
  EXPECT_EQ(level.entry("upper")->pose, valid.entries[1].pose);
  EXPECT_EQ(level.entry(level.defaultEntryId())->pose, valid.entries[0].pose);
  EXPECT_EQ(level.entry("missing"), nullptr);
  EXPECT_EQ(level.entries(), valid.entries);
  EXPECT_EQ(level.defaultEntryId(), "lower");
  for (float height : {-0.1F, 0.1F, 1.5F, 3.1F}) {
    auto doc = valid;
    doc.entries[1].pose.foot_position.y = height;
    EXPECT_FALSE(validateLevelDocument(doc).empty()) << height;
  }
  for (const auto& block : {PrototypeSolid{{0.3F, 3.8F, 0}, {0.1F, 0.8F, 1}},
                            PrototypeSolid{{0, 4.7F, 0}, {1, 0.1F, 1}}}) {
    auto doc = valid;
    doc.solids.push_back(block);
    EXPECT_FALSE(validateLevelDocument(doc).empty());
  }
  auto doc = valid;
  doc.static_prop.translation = {0, 3, 0};
  doc.static_prop.yaw_degrees = 45;
  EXPECT_FALSE(validateLevelDocument(doc).empty());
  doc = valid;
  doc.terrain = PrototypeTerrain{{-24, -1, -24}, 0.5F, {}};
  EXPECT_TRUE(validateLevelDocument(doc).empty());
  doc.terrain->origin.y = 0.1F;
  EXPECT_FALSE(validateLevelDocument(doc).empty());
  doc = valid;
  for (auto& solid : doc.solids) solid.center.x += 100;
  for (auto& entry : doc.entries) entry.pose.foot_position.x += 100;
  doc.terrain = PrototypeTerrain{{-24, 0, -24}, 0.5F, {}};
  doc.static_prop.translation.x = 1000;
  doc.light_switch = PrototypeLightSwitch{{1000, 3, 0}, 0, 0, true};
  EXPECT_TRUE(validateLevelDocument(doc).empty());
  doc.solids[0].half_extent.x = std::numeric_limits<float>::max();
  EXPECT_FALSE(validateLevelDocument(doc).empty());
  doc = valid;
  doc.static_prop.uniform_scale = std::numeric_limits<float>::max();
  EXPECT_FALSE(validateLevelDocument(doc).empty());
}

TEST_F(InteriorLevel,
       JoltAndInitialPresentationUseSelectedEntryWithoutTerrain) {
  const auto doc = floors();
  const auto level = makePrototypeLevel(doc);
  for (const auto& entry : level.entries()) {
    PhysicsWorld physics(level, entry);
    EXPECT_FALSE(physics.hasTerrainCollision());
    EXPECT_EQ(physics.staticBodyCount(), doc.solids.size() + 1);
    EXPECT_TRUE(physics.usesSingleThreadedJobs());
    PlayerController player(physics, entry.pose.yaw_degrees);
    EXPECT_EQ(player.yawDegrees(), entry.pose.yaw_degrees);
    EXPECT_FLOAT_EQ(player.previousPresentation().foot_position.y,
                    entry.pose.foot_position.y);
    EXPECT_FLOAT_EQ(player.currentPresentation().foot_position.y,
                    entry.pose.foot_position.y);
    for (float alpha : {0.0F, 0.5F, 1.0F})
      EXPECT_FLOAT_EQ(player.viewPose(alpha).position.y,
                      entry.pose.foot_position.y + player_standing_eye_height);
    for (int i = 0; i < 60; ++i) player.fixedStep(1.0F / 60);
    EXPECT_TRUE(player.state().supported());
    EXPECT_NEAR(player.state().foot_position.y, entry.pose.foot_position.y,
                0.05F);
  }
  auto foreign = doc.entries[0];
  foreign.pose.foot_position.y = 100;
  EXPECT_THROW(PhysicsWorld(level, foreign), std::invalid_argument);
}

TEST_F(InteriorLevel,
       SupportedTerrainSlopesPermitOrdinaryCapsuleGroundContact) {
  auto doc = interior();
  doc.entries[0].pose.foot_position = {};
  doc.static_prop.translation = {-4, 0, -4};
  for (float slope : {.1F, .5F, 1.0F}) {
    doc.terrain = PrototypeTerrain{{-24, 0, -24}, .5F, {}};
    for (std::size_t z = 0; z < prototype_terrain_sample_count; ++z)
      for (std::size_t x = 0; x < prototype_terrain_sample_count; ++x)
        doc.terrain->heights[z * prototype_terrain_sample_count + x] =
            (static_cast<float>(x) * .5F - 24) * slope;
    ASSERT_TRUE(validateLevelDocument(doc).empty())
        << formatLevelDiagnostics(validateLevelDocument(doc));
    const auto level = makePrototypeLevel(doc);
    PhysicsWorld physics(level);
    PlayerController player(physics);
    for (int i = 0; i < 60; ++i) player.fixedStep(1.0F / 60);
    EXPECT_TRUE(player.state().supported());
    EXPECT_LT(std::abs(player.state().foot_position.y), .3F);
  }
  doc.entries[0].pose.foot_position.y = .1F;
  doc.solids[0].center.y = -.15F;
  EXPECT_FALSE(validateLevelDocument(doc)
                   .empty());  // Adjacent slope enters the capsule.
  doc.terrain.reset();
  EXPECT_TRUE(validateLevelDocument(doc).empty());
}

TEST_F(InteriorLevel, YawedProxyClearanceUsesItsRotatedAxes) {
  auto doc = interior();
  doc.static_prop = {{0, 0, 0},   45,           1, PrototypeSurface::Obstacle,
                     {0, .9F, 0}, {2, .9F, .1F}};
  doc.entries[0].pose.foot_position = {1, 0, 1};
  EXPECT_TRUE(validateLevelDocument(doc).empty());
  doc.entries[0].pose.foot_position.z = -1;
  EXPECT_FALSE(validateLevelDocument(doc).empty());
}

TEST_F(InteriorLevel,
       StrictVersionFourFieldsAndUnsafeBoundsPreserveTheOpenDocument) {
  const auto path = root / "level.json";
  ASSERT_TRUE(saveLevelDocument(path, floors()));
  const auto canonical = bytes(path);
  EditorDocument editor;
  ASSERT_TRUE(editor.open(path));
  const auto before = *editor.document();
  for (const auto& [from, to] :
       {std::pair{"\"version\": 4", "\"version\": 5"},
        std::pair{"\"id\": \"lower\"", "\"id\": false"},
        std::pair{"\"id\": \"lower\"", "\"unknown\": \"lower\""},
        std::pair{"\"default_entry\": \"lower\"", "\"default_entry\": 1"},
        std::pair{"\"x\": 5.0", "\"x\": 3.4e38"},
        std::pair{"\"version\": 4",
                  "\"player_spawn\": null, \"version\": 4"}}) {
    auto bad = canonical;
    const auto offset = bad.find(from);
    ASSERT_NE(offset, std::string::npos);
    bad.replace(offset, std::string_view(from).size(), to);
    write(root / "bad.json", bad);
    EXPECT_FALSE(editor.open(root / "bad.json"));
    EXPECT_EQ(*editor.document(), before);
  }
  auto too_many = canonical;
  const auto begin = too_many.find("\"entries\": [") +
                     std::string_view("\"entries\": [").size();
  const auto end = too_many.find("\n  ],", begin);
  std::string values;
  for (int i = 0; i < 17; ++i) values += i ? ",null" : "null";
  too_many.replace(begin, end - begin, values);
  write(root / "bad.json", too_many);
  EXPECT_FALSE(editor.open(root / "bad.json"));
  EXPECT_EQ(*editor.document(), before);
}

TEST_F(InteriorLevel, EmptyEditorMeshAndInteriorGeometryKeepStructuralBounds) {
  const auto doc = floors();
  const auto vertices = buildPrototypeSceneVertices(doc.terrain, doc.solids);
  EXPECT_EQ(vertices.size(), doc.solids.size() * 36);
  EXPECT_TRUE(buildPrototypeSceneVertices(std::nullopt, {}).empty());
  for (std::size_t i = 0; i < doc.solids.size(); ++i) {
    const auto& box = doc.solids[i];
    for (std::size_t j = 0; j < 36; ++j) {
      const auto& v = vertices[i * 36 + j];
      EXPECT_FLOAT_EQ(std::abs(v.position[0] - box.center.x),
                      box.half_extent.x);
      EXPECT_FLOAT_EQ(std::abs(v.position[1] - box.center.y),
                      box.half_extent.y);
      EXPECT_FLOAT_EQ(std::abs(v.position[2] - box.center.z),
                      box.half_extent.z);
    }
  }
}

TEST_F(InteriorLevel, EntryCommandsKeepDurableReferencesAndHistoryAtomic) {
  EditorDocument editor;
  editor.requestNewInterior();
  EXPECT_TRUE(editor.dirty());
  EXPECT_TRUE(editor.valid());
  EXPECT_FALSE(editor.path());
  EXPECT_FALSE(editor.document()->terrain);
  ASSERT_TRUE(editor.saveAs(root / "interior.json"));
  const auto original = *editor.document();
  editor.select(editor.entryIds()[0]);
  EXPECT_FALSE(editor.removeSelected());
  ASSERT_TRUE(editor.duplicateSelected());
  const auto duplicate = editor.selection();
  auto entry = std::get<LevelEntry>(*editor.object(duplicate));
  EXPECT_EQ(entry.id, "entry-1");
  EXPECT_EQ(entry.pose, original.entries[0].pose);
  ASSERT_TRUE(editor.makeSelectedEntryDefault());
  ASSERT_TRUE(editor.selectLaunchEntry(entry.id));
  entry.id = "landing";
  ASSERT_TRUE(editor.replaceObject(duplicate, entry));
  EXPECT_EQ(editor.document()->default_entry, "landing");
  EXPECT_EQ(editor.launchEntry(), "landing");
  EXPECT_FALSE(editor.removeSelected());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.launchEntry(), "entry-1");
  EXPECT_EQ(editor.document()->default_entry, "entry-1");
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.document()->default_entry, "default");
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(editor.launchEntry(), "default");
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(editor.selection(), duplicate);
  ASSERT_TRUE(editor.selectLaunchEntry("entry-1"));
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_EQ(editor.launchEntry(), "default");
  ASSERT_TRUE(editor.undo());
  entry.id = "default";
  EXPECT_FALSE(editor.replaceObject(duplicate, entry));
  for (int i = 2; i < 16; ++i)
    ASSERT_TRUE(editor.addEntry(original.entries[0].pose));
  EXPECT_FALSE(editor.addEntry(original.entries[0].pose));
  ASSERT_TRUE(editor.save());
  EXPECT_TRUE(editor.selectLaunchEntry("entry-1"));
  EXPECT_FALSE(editor.dirty());
}

TEST_F(InteriorLevel,
       NewInteriorUsesPendingReplacementAndSafeInvalidFilesStayEditable) {
  EditorDocument editor;
  editor.requestNewInterior();
  const auto original = *editor.document();
  editor.requestNewInterior();
  EXPECT_EQ(editor.pendingAction().kind, EditorPendingActionKind::NewInterior);
  EXPECT_FALSE(editor.resolvePending(EditorPendingDecision::Save));
  EXPECT_EQ(*editor.document(), original);
  EXPECT_TRUE(editor.resolvePending(EditorPendingDecision::Cancel));
  ASSERT_TRUE(editor.addEntry(original.entries[0].pose));
  editor.requestNewInterior();
  EXPECT_TRUE(editor.resolvePending(EditorPendingDecision::Discard));
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.terrainStrokeActive());
  editor.beginTerrainStroke({{0, 0, 0}});
  EXPECT_FALSE(editor.terrainStrokeActive());
  ASSERT_TRUE(editor.saveAs(root / "valid.json"));
  auto unsafe = bytes(root / "valid.json");
  unsafe.replace(unsafe.find("\"default_entry\": \"default\""), 26,
                 "\"default_entry\": \"missing\"");
  write(root / "invalid.json", unsafe);
  ASSERT_TRUE(editor.open(root / "invalid.json"));
  EXPECT_FALSE(editor.valid());
  EXPECT_FALSE(editor.dirty());
  EXPECT_FALSE(editor.save());
  const auto invalid = *editor.document();
  write(root / "malformed.json", "{");
  EXPECT_FALSE(editor.open(root / "malformed.json"));
  EXPECT_EQ(*editor.document(), invalid);
}

TEST_F(InteriorLevel,
       SurfacePickingUsesActualNearestFaceAndSuppressesUnavailablePlacement) {
  ASSERT_TRUE(saveLevelDocument(root / "floors.json", floors()));
  EditorDocument editor;
  ASSERT_TRUE(editor.open(root / "floors.json"));
  editor.select(editor.entryIds()[0]);
  const EditorRay ray{{1, 8, 1}, {0, -1, 0}};
  const auto hit =
      pickEditorSurface(editor, ray, EditorPlacementMode::SceneSurfaces);
  ASSERT_TRUE(hit);
  EXPECT_EQ(hit->target, editor.solidIds()[1]);
  EXPECT_EQ(hit->face, EditorSurfaceFace::Top);
  EXPECT_EQ(hit->position, (WorldPosition{1, 3, 1}));
  EXPECT_EQ(hit->normal, (WorldPosition{0, 1, 0}));
  EXPECT_FALSE(
      pickEditorSurface(editor, ray, EditorPlacementMode::TerrainOnly));
  const auto before = *editor.document();
  for (const auto [capture, navigation] :
       {std::pair{true, false}, std::pair{false, true}}) {
    EXPECT_FALSE(
        updateEditorPlacementViewport(editor, ray, capture, navigation, true,
                                      EditorPlacementMode::SceneSurfaces, {}));
    EXPECT_EQ(*editor.document(), before);
  }
  ASSERT_TRUE(updateEditorPlacementViewport(
      editor, ray, false, false, true, EditorPlacementMode::SceneSurfaces, {}));
  EXPECT_EQ(editor.document()->entries[0].pose.foot_position, hit->position);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  const EditorRay underside{{0, 1.5F, 0}, {0, 1, 0}};
  const auto blocked =
      updateEditorPlacementViewport(editor, underside, false, false, true,
                                    EditorPlacementMode::SceneSurfaces, {});
  ASSERT_TRUE(blocked);
  EXPECT_EQ(blocked->face, EditorSurfaceFace::Bottom);
  EXPECT_EQ(*editor.document(), before);
  editor.select(editor.solidIds()[1]);
  EXPECT_EQ(pickEditorSurface(editor, ray, EditorPlacementMode::SceneSurfaces)
                ->target,
            editor.solidIds()[0]);
  editor.select(editor.entryIds()[0]);
  EXPECT_EQ(pickEditorObject(editor, {{0, 4, 0}, {0, -1, 0}}),
            editor.entryIds()[1]);
  const auto upper_id = editor.solidIds()[1];
  ASSERT_TRUE(editor.addSolid(editor.document()->solids[1]));
  editor.select(editor.entryIds()[0]);
  EXPECT_EQ(pickEditorSurface(editor, ray, EditorPlacementMode::SceneSurfaces)
                ->target,
            upper_id);
}

TEST_F(InteriorLevel,
       FloorAndWallAnchorsPreservePropertiesAndMountSwitchOutward) {
  const auto doc = interior();
  const EditorSurfaceHit floor{
      {1, 3, 2}, {0, 1, 0}, 2, 99, EditorSurfaceFace::Top};
  EXPECT_EQ(
      std::get<PrototypeSolid>(*editorPlacedObject(doc.solids[0], floor, {}))
          .center.y,
      3.25F);
  EXPECT_EQ(std::get<PrototypeStaticProp>(
                *editorPlacedObject(doc.static_prop, floor, {}))
                .translation,
            floor.position);
  const auto light = doc.environment_light.point_lights[0];
  EXPECT_EQ(std::get<PrototypePointLight>(
                *editorPlacedObject(light, floor, {1.8F, .1F}))
                .position.y,
            4.8F);
  const PrototypeLightSwitch value{{}, 77, 1, false};
  for (const WorldPosition normal :
       {WorldPosition{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}) {
    const EditorSurfaceHit wall{
        {2, 1.4F, 3}, normal, 1, 33, EditorSurfaceFace::PositiveX};
    EXPECT_FALSE(editorPlacedObject(doc.entries[0], wall, {}));
    EXPECT_FALSE(editorPlacedObject(doc.static_prop, wall, {}));
    const auto solid =
        std::get<PrototypeSolid>(*editorPlacedObject(doc.solids[0], wall, {}));
    EXPECT_FLOAT_EQ(solid.center.x,
                    wall.position.x + normal.x * doc.solids[0].half_extent.x);
    EXPECT_FLOAT_EQ(solid.center.z,
                    wall.position.z + normal.z * doc.solids[0].half_extent.z);
    const auto placed =
        std::get<PrototypeLightSwitch>(*editorPlacedObject(value, wall, {}));
    const auto back =
        lightSwitchWorldPoint(placed, {0, 0, -light_switch_half_extent.z});
    EXPECT_NEAR(back.x, wall.position.x + normal.x * .001F, .00001F);
    EXPECT_NEAR(back.z, wall.position.z + normal.z * .001F, .00001F);
    EXPECT_EQ(placed.point_light_index, 1U);
    EXPECT_FALSE(placed.initially_on);
    EXPECT_EQ(editorPlacedObject(placed, wall, {}),
              editorPlacedObject(value, wall, {}));
    const auto mounted_light = std::get<PrototypePointLight>(
        *editorPlacedObject(light, wall, {2, .2F}));
    EXPECT_NEAR(mounted_light.position.x, wall.position.x + normal.x * .2F,
                .00001F);
    EXPECT_EQ(mounted_light.color, light.color);
  }
}

TEST(LaunchOptions, NativePathsAndStrictOptionsAreResolvedBeforeStartup) {
  near_laugh::RuntimeConfig config;
  const auto cwd = std::filesystem::absolute("build");
  launcher::applyLaunchOptions(config, {}, cwd);
  EXPECT_FALSE(config.level_path);
  EXPECT_FALSE(config.entry_id);
  near_laugh::RuntimeConfig entry_only;
  const std::vector<std::filesystem::path> entry_args{"--entry", "upper"};
  launcher::applyLaunchOptions(entry_only, entry_args, cwd);
  EXPECT_FALSE(entry_only.level_path);
  EXPECT_EQ(entry_only.entry_id, "upper");
  near_laugh::RuntimeConfig level_only;
  const std::vector<std::filesystem::path> level_args{"--level",
                                                      cwd / "level.json"};
  launcher::applyLaunchOptions(level_only, level_args, cwd);
  EXPECT_EQ(level_only.level_path, cwd / "level.json");
  EXPECT_FALSE(level_only.entry_id);
  const std::filesystem::path relative{u8"levels/комната & literal;.json"};
  const std::vector<std::filesystem::path> good{"--entry", "upper", "--level",
                                                relative};
  launcher::applyLaunchOptions(config, good, cwd);
  EXPECT_EQ(config.level_path, (cwd / relative).lexically_normal());
  EXPECT_EQ(config.entry_id, "upper");
  const auto original = config;
  for (const std::vector<std::filesystem::path>& args :
       {std::vector<std::filesystem::path>{"--unknown"},
        {"--level"},
        {"--entry"},
        {"--level", ""},
        {"--entry", ""},
        {"--entry", "Upper"},
        {"--entry", "x", "--entry", "y"},
        {"--level", "x", "--level", "y"},
        {"--level", "--entry", "upper"}}) {
    EXPECT_THROW(launcher::applyLaunchOptions(config, args, cwd),
                 std::invalid_argument);
    EXPECT_EQ(config.level_path, original.level_path);
    EXPECT_EQ(config.entry_id, original.entry_id);
  }
}

TEST_F(InteriorLevel,
       TerrainStrokeRevalidatesEveryEntryWhileUpperSupportRemainsClear) {
  auto doc = floors();
  doc.terrain = PrototypeTerrain{{-24, 0, -24}, .5F, {}};
  ASSERT_TRUE(saveLevelDocument(root / "floors.json", doc));
  EditorDocument editor;
  ASSERT_TRUE(editor.open(root / "floors.json"));
  editor.beginTerrainStroke({{0, 0, 0}});
  ASSERT_TRUE(editor.finishTerrainStroke());
  EXPECT_FALSE(editor.valid());
  bool lower_reported = false;
  for (const auto& diagnostic : editor.diagnostics()) {
    if (diagnostic.document_path.starts_with("entries[0]"))
      lower_reported = true;
    EXPECT_FALSE(diagnostic.document_path.starts_with("entries[1]"));
  }
  EXPECT_TRUE(lower_reported);
  EXPECT_EQ(editor.document()->entries, doc.entries);
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.valid());
  EXPECT_FALSE(editor.dirty());
}

TEST_F(InteriorLevel, PackagedApartmentStairsWalkBothDirectionsFromBothStarts) {
  const auto path =
      std::filesystem::absolute("resources/levels/apartment-stairs.level.json");
  const auto source = bytes(path);
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded);
  ASSERT_TRUE(saveLevelDocument(root / "apartment.json", *loaded.document));
  EXPECT_EQ(bytes(root / "apartment.json"), source);
  const auto level = makePrototypeLevel(*loaded.document);
  EXPECT_FALSE(level.terrain());
  EXPECT_EQ(level.defaultEntryId(), "apartment");
  for (const auto& entry : level.entries()) {
    PhysicsWorld physics(level, entry);
    auto state = physics.characterState();
    const auto step = [&](float x, float z) {
      constexpr float dt = 1.0F / 60;
      const float vy =
          (state.supported() ? 0.0F : state.linear_velocity.y) - 18 * dt;
      state = physics.stepCharacter({{x, vy, z}, {0, -18, 0}, false}, dt);
    };
    const auto walk = [&](WorldPosition target) {
      for (int i = 0; i < 1800; ++i) {
        const float dx = target.x - state.foot_position.x;
        const float dz = target.z - state.foot_position.z;
        const float distance = std::hypot(dx, dz);
        if (distance < .06F) break;
        const float speed = std::min(2.0F, distance * 60);
        step(speed * dx / distance, speed * dz / distance);
        EXPECT_EQ(state.stance, PhysicsPlayerStance::Standing);
      }
      for (int i = 0; i < 30; ++i) step(0, 0);
      EXPECT_NEAR(state.foot_position.x, target.x, .08F);
      EXPECT_NEAR(state.foot_position.z, target.z, .08F);
      EXPECT_NEAR(state.foot_position.y, target.y, .06F);
      EXPECT_TRUE(state.supported());
    };
    SCOPED_TRACE(entry.id);
    if (entry.id == "apartment")
      walk({0, 3, 3.7F});
    else
      walk({0, 3, -5});
    walk({0, 3, -2.3F});
    walk({2.5F, 3, -2.3F});  // Kitchen through its authored doorway.
    walk({0, 3, -2.3F});
    walk({0, 0, -16.2F});
    walk({0, 3, -5});
    walk({0, 3, 3.7F});
    walk({-3, 3, 3.7F});  // Lena's room through its authored doorway.
  }
  EXPECT_EQ(bytes(path), source);
}
