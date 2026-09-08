#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <fstream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>

#include "core/audio/apartment_audio_fixture.hpp"
#include "core/audio/audio_playback.hpp"
#include "core/audio/cue_coordinator.hpp"
#include "core/audio/transmission.hpp"
#include "core/text/caption_font.hpp"
#include "core/world/audio.hpp"
#include "core/world/prototype_level.hpp"
#include "editor/editor_document.hpp"
#include "prototype_level_fixture.hpp"

namespace {
LevelAudio authoredAudio() {
  LevelAudio a;
  a.cues = {{"radio-cue", "radio", "radio", AudioCueKind::Ambience, true, true},
            {"phone-cue", "phone-conversation", "phone-conversation",
             AudioCueKind::Dialogue, false, true}};
  a.sources = {{"radio-source", "radio-cue", {2, 1, 0}, 0.5F, 1, 20, true},
               {"phone-source", "phone-cue", {}, 1, 1, 20, false}};
  a.rooms = {{"room-a", {}, {2, 2, 2}}, {"room-b", {4, 0, 0}, {2, 2, 2}}};
  a.connections = {{"door-link", "room-a", "room-b", "room-door", 0.2F, 1},
                   {"outside-link", "room-b", {}, {}, 0.5F, 0.8F}};
  return a;
}
const std::array<DoorDefinition, 1> audioDoors{{{"room-door"}}};
bool rejects(const LevelAudio& a, std::string_view field) {
  const auto errors = validateLevelAudio(a, audioDoors);
  for (const auto& e : errors)
    if (e.document_path.find(field) != std::string::npos) return true;
  return false;
}
}  // namespace

TEST(AudioDefinitions, EmptyAndCompleteProfilesNeedNoDevicesOrResourceFiles) {
  EXPECT_TRUE(validateLevelAudio({}, {}).empty());
  EXPECT_TRUE(validateLevelAudio(authoredAudio(), audioDoors).empty());
}

TEST(AudioDefinitions, CollectionBoundsAndIdentityAreValidatedIndependently) {
  auto a = authoredAudio();
  a.cues.push_back(a.cues.front());
  EXPECT_TRUE(rejects(a, "cues[2].id"));
  a = authoredAudio();
  a.sources.front().id = "Invalid/id";
  EXPECT_TRUE(rejects(a, "sources[0].id"));
  a = authoredAudio();
  a.rooms.push_back(a.rooms.front());
  EXPECT_TRUE(rejects(a, "rooms[2].id"));
  a = authoredAudio();
  a.connections.push_back(a.connections.front());
  EXPECT_TRUE(rejects(a, "connections[2].id"));
  a.cues.resize(129);
  a.sources.resize(65);
  a.rooms.resize(33);
  a.connections.resize(65);
  EXPECT_TRUE(rejects(a, "audio.cues"));
  EXPECT_TRUE(rejects(a, "audio.sources"));
  EXPECT_TRUE(rejects(a, "audio.rooms"));
  EXPECT_TRUE(rejects(a, "audio.connections"));
}

TEST(AudioDefinitions, EveryCollectionCanUseItsFullCapacity) {
  LevelAudio a;
  for (int i = 0; i < 128; ++i)
    a.cues.push_back({"cue-" + std::to_string(i),
                      "radio",
                      {},
                      AudioCueKind::Ambience,
                      true,
                      false});
  for (int i = 0; i < 64; ++i)
    a.sources.push_back(
        {"source-" + std::to_string(i), "cue-0", {}, 1, 1, 100, true});
  for (int i = 0; i < 32; ++i)
    a.rooms.push_back(
        {"room-" + std::to_string(i), {float(i * 2), 0, 0}, {1, 1, 1}});
  for (int i = 0; i < 64; ++i)
    a.connections.push_back(
        {"connection-" + std::to_string(i), "room-0", {}, {}, 0, 1});
  EXPECT_TRUE(validateLevelAudio(a, {}).empty());
}

TEST(AudioDefinitions, ForegroundCaptionsAutoplayAndCatalogAreChecked) {
  auto a = authoredAudio();
  a.cues[1].caption.reset();
  EXPECT_TRUE(rejects(a, "cues[1].caption"));
  a.cues[1].loop = true;
  EXPECT_TRUE(rejects(a, "cues[1].loop"));
  a.sources[1].autoplay = true;
  EXPECT_TRUE(rejects(a, "sources[1].autoplay"));
  a.cues[0].clip = "../file.wav";
  EXPECT_TRUE(rejects(a, "cues[0].clip"));
  a.cues[0].caption = "unknown";
  EXPECT_TRUE(rejects(a, "cues[0].caption"));
  a.sources[0].cue = "missing";
  EXPECT_TRUE(rejects(a, "sources[0].cue"));
}

TEST(AudioDefinitions, InvalidNumbersAndUnrepresentableRoomBoundsAreRejected) {
  auto a = authoredAudio();
  for (float bad : {-1.0F, 1.1F, std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::quiet_NaN()}) {
    a.sources[0].gain = bad;
    EXPECT_TRUE(rejects(a, "sources[0].gain"));
    a.connections[0].open_gain = bad;
    EXPECT_TRUE(rejects(a, "connections[0].closed_gain"));
  }
  a = authoredAudio();
  a.sources[0].near_distance = 0;
  EXPECT_TRUE(rejects(a, "sources[0].near_distance"));
  a.sources[0].near_distance = 2;
  a.sources[0].far_distance = 2;
  EXPECT_TRUE(rejects(a, "sources[0].near_distance"));
  a.sources[0].far_distance = 101;
  EXPECT_TRUE(rejects(a, "sources[0].near_distance"));
  a.rooms[0].center.x = std::numeric_limits<float>::max();
  a.rooms[0].half_extent.x = std::numeric_limits<float>::max();
  EXPECT_TRUE(rejects(a, "rooms[0].bounds"));
  a.rooms[0].half_extent.x = 1;
  EXPECT_TRUE(rejects(a, "rooms[0].bounds"));
}

TEST(AudioDefinitions, SharedFacesAreValidButOverlappingInteriorsAreNot) {
  auto a = authoredAudio();
  EXPECT_TRUE(validateLevelAudio(a, audioDoors).empty());
  a.rooms[1].center.x = 3.9F;
  EXPECT_TRUE(rejects(a, "rooms[1].bounds"));
  a.rooms[1].center = {0, 4, 0};
  EXPECT_TRUE(validateLevelAudio(a, audioDoors).empty());
  a.rooms[1].half_extent.y = 0;
  EXPECT_TRUE(rejects(a, "rooms[1].bounds"));
}

TEST(AudioDefinitions,
     BrokenLinksAndDuplicateDoorConnectionsRemainDiagnosable) {
  auto a = authoredAudio();
  a.connections[0].room_a = "missing";
  EXPECT_TRUE(rejects(a, "connections[0].room_a"));
  a.connections[0].room_b = "missing";
  EXPECT_TRUE(rejects(a, "connections[0].room_b"));
  a.connections[0].door = "missing";
  EXPECT_TRUE(rejects(a, "connections[0].door"));
  a = authoredAudio();
  a.connections[1].door = "room-door";
  EXPECT_TRUE(rejects(a, "connections[1].door"));
  a.connections[1].closed_gain = 0.9F;
  EXPECT_TRUE(rejects(a, "connections[1].closed_gain"));
}

namespace {
class AudioPersistence : public testing::Test {
 protected:
  std::filesystem::path root;
  void SetUp() override {
    root = std::filesystem::temp_directory_path() /
           ("audio-level-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
  }
  void TearDown() override { std::filesystem::remove_all(root); }
  std::string read(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
  }
  void write(const std::filesystem::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary);
    f << text;
  }
};
}  // namespace

TEST_F(AudioPersistence,
       OrderedAudioAndAllPriorFieldsSurviveCanonicalRoundTrips) {
  auto document = prototypeLevelDocument();
  document.audio = authoredAudio();
  document.audio.connections[0].door.reset();
  const auto file = root / "roundtrip.json";
  ASSERT_TRUE(saveLevelDocument(file, document));
  const auto bytes = read(file);
  const auto loaded = loadLevelDocument(file);
  ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  EXPECT_EQ(loaded.source_version, 9U);
  EXPECT_EQ(*loaded.document, document);
  EXPECT_EQ(makePrototypeLevel(*loaded.document).audio(), document.audio);
  ASSERT_TRUE(saveLevelDocument(file, *loaded.document));
  EXPECT_EQ(read(file), bytes);
  document.audio.sources[0].cue = "missing";
  EXPECT_FALSE(saveLevelDocument(file, document));
  EXPECT_EQ(read(file), bytes);
  EXPECT_THROW(static_cast<void>(makePrototypeLevel(document)),
               std::invalid_argument);
}

TEST_F(AudioPersistence, V6OpensCleanAndExplicitSavePreservesAllPriorData) {
  const auto old = read("tests/fixtures/levels/prototype-v6.level.json");
  const auto file = root / "legacy.json";
  write(file, old);
  EditorDocument editor;
  ASSERT_TRUE(editor.open(file));
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(editor.sourceVersion(), 6U);
  EXPECT_EQ(editor.document()->audio, LevelAudio{});
  const auto expected = *editor.document();
  EXPECT_EQ(read(file), old);
  ASSERT_TRUE(editor.save());
  const auto loaded = loadLevelDocument(file);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded.source_version, 9U);
  EXPECT_EQ(*loaded.document, expected);
}

TEST_F(AudioPersistence,
       VersionShapesAndAudioFieldTypesAreStrictAndTransactional) {
  auto doc = prototypeLevelDocument();
  doc.audio = authoredAudio();
  doc.audio.connections[0].door.reset();
  const auto good = root / "good.json";
  ASSERT_TRUE(saveLevelDocument(good, doc));
  const auto canonical = read(good);
  EditorDocument editor;
  ASSERT_TRUE(editor.open(good));
  const auto reject = [&](const std::string& text, std::string_view field) {
    const auto bad = root / "bad.json";
    write(bad, text);
    const auto loaded = loadLevelDocument(bad);
    ASSERT_FALSE(loaded);
    EXPECT_NE(formatLevelDiagnostics(loaded.diagnostics).find(field),
              std::string::npos)
        << formatLevelDiagnostics(loaded.diagnostics);
    EXPECT_FALSE(editor.open(bad));
    EXPECT_EQ(*editor.document(), doc);
    EXPECT_FALSE(editor.dirty());
  };
  for (auto [from, to, field] : std::array<std::array<const char*, 3>, 5>{
           {{"\"audio\"", "\"unsupported_audio\"", "unsupported_audio"},
            {"\"loop\": true", "\"loop\": 1", "loop"},
            {"\"near_distance\": 1.0", "\"near_distance\": \"nan\"",
             "near_distance"},
            {"\"room_a\": \"room-a\"", "\"room_a\": false", "room_a"},
            {"\"kind\": \"ambience\"", "\"kind\": \"unknown\"", "kind"}}}) {
    auto text = canonical;
    const auto at = text.find(from);
    ASSERT_NE(at, std::string::npos);
    text.replace(at, std::string_view(from).size(), to);
    reject(text, field);
  }
  auto missing = canonical;
  missing.erase(missing.find(",\n  \"audio\""));
  missing += "\n}\n";
  reject(missing, "audio");
  for (int version : {3, 4, 5, 6}) {
    auto text = read("tests/fixtures/levels/prototype-v" +
                     std::to_string(version) + ".level.json");
    text.insert(text.find('{') + 1, "\n\"audio\": {},");
    reject(text, "audio");
    if (version == 3) {
      text.replace(text.find("\"version\": 3"), 12, "\"version\": 2");
      reject(text, "audio");
    }
  }
}

static_assert(!std::is_copy_constructible_v<AudioPlayback>);
static_assert(!std::is_move_constructible_v<AudioPlayback>);

TEST(AudioPlayback, EmptyOfflineEngineRendersSilenceAndCanBeRecreated) {
  for (int run = 0; run != 3; ++run) {
    AudioPlayback playback(AudioOutput::Offline);
    ASSERT_FALSE(playback.silent());
    EXPECT_TRUE(playback.warning().empty());
    std::array<float, 960> pcm;
    pcm.fill(1.0F);
    playback.render(pcm);
    for (float sample : pcm) EXPECT_EQ(sample, 0.0F);
  }
}

TEST(AudioPlayback, ExplicitSilentOutputClearsSamplesWithoutADevice) {
  AudioPlayback playback(AudioOutput::Silent);
  EXPECT_TRUE(playback.silent());
  std::array<float, 960> pcm;
  pcm.fill(1.0F);
  playback.render(pcm);
  for (float sample : pcm) EXPECT_EQ(sample, 0.0F);
  std::array<float, 3> partial_frame{};
  EXPECT_THROW(playback.render(partial_frame), std::invalid_argument);
}

namespace {
AudioContent radioContent() {
  LevelAudio a;
  a.cues = {authoredAudio().cues[0]};
  return prepareAudioContent("resources", a);
}
std::array<double, 2> renderEnergy(AudioPlayback& playback) {
  std::array<float, 48000> pcm{};
  playback.render(pcm);
  std::array<double, 2> energy{};
  // Allow the specified 50 ms gain smoothing to settle.
  for (std::size_t i = 9600; i < pcm.size(); ++i)
    energy[i % 2] += double(pcm[i]) * pcm[i];
  return energy;
}
}  // namespace

TEST(AudioPlayback,
     RealOfflinePcmTracksListenerRotationDistanceAndMovingSources) {
  AudioPlayback playback(radioContent(), AudioOutput::Offline);
  auto source = authoredAudio().sources[0];
  source.position = {3, 0, -3};
  playback.listener({}, {0, 0, -1});
  playback.start(0, authoredAudio().cues[0], source);
  const auto right = renderEnergy(playback);
  EXPECT_GT(right[1], right[0] * 1.1);
  EXPECT_GT(right[1], 0.1);
  const auto cursor = playback.cursor(0);
  playback.listener({}, {0, 0, 1});
  const auto left = renderEnergy(playback);
  EXPECT_GT(left[0], left[1] * 1.1);
  playback.source(0, {0, 0, -1}, 1);
  const auto near = renderEnergy(playback);
  EXPECT_GT(playback.cursor(0), cursor + 0.8);
  playback.source(0, {0, 0, -15}, 1);
  const auto far = renderEnergy(playback);
  EXPECT_LT(far[0] + far[1], (near[0] + near[1]) * 0.2);
  playback.source(0, {0, 0, -30}, 1);
  const auto beyond = renderEnergy(playback);
  EXPECT_NEAR(beyond[0] + beyond[1], 0, 0.00001);
  EXPECT_EQ(playback.voiceCount(), 1U);
}

TEST(AudioPlayback, MuteAdvancesCursorSuspensionFreezesAndStopReleasesVoices) {
  AudioPlayback playback(radioContent(), AudioOutput::Offline);
  const auto a = authoredAudio();
  playback.start(0, a.cues[0], a.sources[0]);
  playback.mute(true);
  const auto muted = renderEnergy(playback);
  EXPECT_EQ(muted[0] + muted[1], 0);
  EXPECT_NEAR(playback.cursor(0), .5, .01);
  playback.suspend(true);
  static_cast<void>(renderEnergy(playback));
  EXPECT_NEAR(playback.cursor(0), .5, .01);
  playback.suspend(false);
  playback.mute(false);
  const auto resumed = renderEnergy(playback);
  EXPECT_GT(resumed[0] + resumed[1], 0.1);
  EXPECT_TRUE(playback.seek(0, 2));
  static_cast<void>(renderEnergy(playback));
  EXPECT_NEAR(playback.cursor(0), 2.5, .02);
  for (std::size_t i = 1; i < 64; ++i)
    playback.start(i, a.cues[0], a.sources[0]);
  EXPECT_EQ(playback.voiceCount(), 64U);
  EXPECT_THROW(playback.start(64, a.cues[0], a.sources[0]), std::out_of_range);
  for (std::size_t i = 0; i < 64; ++i) playback.stop(i);
  EXPECT_EQ(playback.voiceCount(), 0U);
  playback.stop(0);
}

TEST(AudioPlayback, SilentFallbackReleasesVoicesAndRetainsPreparedContent) {
  AudioPlayback playback(radioContent(), AudioOutput::Offline);
  const auto a = authoredAudio();
  playback.start(0, a.cues[0], a.sources[0]);
  static_cast<void>(renderEnergy(playback));
  playback.silentFallback("Output device lost; restart to retry.");
  EXPECT_TRUE(playback.silent());
  EXPECT_FALSE(playback.warning().empty());
  EXPECT_EQ(playback.voiceCount(), 0U);
  EXPECT_EQ(playback.content().clip("radio").duration(), 10);
  playback.start(0, a.cues[0], a.sources[0]);
  EXPECT_EQ(playback.voiceCount(), 0U);
  EXPECT_EQ(renderEnergy(playback), (std::array<double, 2>{}));
}

TEST(AudioTransmission, HalfOpenMembershipDistinguishesRoomsFloorsAndOutside) {
  auto a = authoredAudio();
  EXPECT_EQ(audioRoomAt(a, {1.999F, 0, 0}), "room-a");
  EXPECT_EQ(audioRoomAt(a, {2, 0, 0}), "room-b");
  EXPECT_FALSE(audioRoomAt(a, {6, 0, 0}));
  a.rooms[1].center = {0, 4, 0};
  EXPECT_EQ(audioRoomAt(a, {0, 2, 0}), "room-b");
  a.rooms[1]
      .center = {};  // Residual ties are stable even in an invalid preview.
  std::reverse(a.rooms.begin(), a.rooms.end());
  EXPECT_EQ(audioRoomAt(a, {}), "room-a");
}

TEST(AudioTransmission, AcceptedNegativeDoorAnglesAndLockOnlyChanges) {
  const auto a = authoredAudio();
  auto doors = audioDoors;
  doors[0].open_angle_degrees = -90;
  const WorldPosition room_b{4, 0, 0};
  EXPECT_FLOAT_EQ(audioTransmission(a, doors, std::array{0.0F}, {}, room_b),
                  .2F);
  EXPECT_FLOAT_EQ(audioTransmission(a, doors, std::array{-45.0F}, {}, room_b),
                  .6F);
  doors[0].initially_locked = true;
  EXPECT_FLOAT_EQ(audioTransmission(a, doors, std::array{-45.0F}, {}, room_b),
                  .6F);
  EXPECT_FLOAT_EQ(audioTransmission(a, doors, std::array{-90.0F}, {}, room_b),
                  1);
  EXPECT_FLOAT_EQ(audioTransmission(a, doors, {}, {}, {1, 0, 0}), 1);
}

TEST(AudioTransmission, StrongestAlternatePathWinsAndCyclesCannotAmplify) {
  auto a = authoredAudio();
  a.connections.push_back({"alternate", "room-a", {}, {}, 1, 1});
  EXPECT_FLOAT_EQ(audioTransmission(a, audioDoors, {}, {}, {4, 0, 0}), .8F);
  std::reverse(a.connections.begin(), a.connections.end());
  EXPECT_FLOAT_EQ(audioTransmission(a, audioDoors, {}, {}, {4, 0, 0}), .8F);
  a.connections.clear();
  EXPECT_FLOAT_EQ(audioTransmission(a, audioDoors, {}, {}, {4, 0, 0}), .1F);
  EXPECT_FLOAT_EQ(
      audioTransmission(a, audioDoors, {}, {100, 0, 0}, {200, 0, 0}), 1);
  EXPECT_FLOAT_EQ(audioDistanceGain({}, {1, 0, 0}, 1, 11), 1);
  EXPECT_FLOAT_EQ(audioDistanceGain({}, {6, 0, 0}, 1, 11), .5F);
  EXPECT_FLOAT_EQ(audioDistanceGain({}, {12, 0, 0}, 1, 11), 0);
}

TEST_F(AudioPersistence,
       SelectedResourcesAreDeduplicatedAndUnselectedFilesAreNotRequired) {
  auto a = authoredAudio();
  a.cues.resize(1);
  std::filesystem::create_directories(root / "audio");
  std::filesystem::create_directories(root / "captions");
  std::filesystem::copy_file("resources/audio/radio.wav",
                             root / "audio/radio.wav");
  std::filesystem::copy_file("resources/captions/radio.captions",
                             root / "captions/radio.captions");
  a.cues.push_back(a.cues[0]);
  a.cues.back().id = "second-cue";
  const auto content = prepareAudioContent(root, a);
  EXPECT_EQ(content.clips.size(), 1U);
  EXPECT_EQ(content.captions.size(), 1U);
  EXPECT_EQ(content.clip("radio").channels, 1U);
  EXPECT_EQ(content.clip("radio").duration(), 10);
  a.cues.push_back(authoredAudio().cues[1]);
  try {
    static_cast<void>(prepareAudioContent(root, a));
    FAIL();
  } catch (const std::runtime_error& e) {
    EXPECT_NE(std::string(e.what()).find("phone-cue"), std::string::npos);
    EXPECT_NE(std::string(e.what()).find("phone-conversation.wav"),
              std::string::npos);
  }
}

TEST_F(AudioPersistence, CorruptStereoAndUnsupportedPcmFailBeforePlayback) {
  auto a = authoredAudio();
  a.cues.resize(1);
  a.cues[0].caption.reset();
  std::filesystem::create_directories(root / "audio");
  const auto original = read("resources/audio/radio.wav");
  for (int defect = 0; defect < 5; ++defect) {
    auto bytes = original;
    if (defect == 0) bytes[0] = '?';
    if (defect == 1) bytes.pop_back();
    if (defect == 2) bytes[24] = 0;  // sample rate
    if (defect == 3) bytes[20] = 3;  // float encoding
    if (defect == 4) {  // Valid stereo profile, invalid for spatial playback.
      bytes[22] = 2;
      bytes[28] = char(0);
      bytes[29] = char(0xee);
      bytes[30] = 2;
      bytes[32] = 4;
    }
    write(root / "audio/radio.wav", bytes);
    EXPECT_THROW(static_cast<void>(prepareAudioContent(root, a)),
                 std::runtime_error);
  }
}

TEST_F(AudioPersistence, CaptionsRejectMalformedUtf8BadTimingAndOverlongText) {
  auto a = authoredAudio();
  a.cues.resize(1);
  std::filesystem::create_directories(root / "audio");
  std::filesystem::create_directories(root / "captions");
  std::filesystem::copy_file("resources/audio/radio.wav",
                             root / "audio/radio.wav");
  for (const auto& bad : std::vector<std::string>{
           "0\t1\tRadio\t\xc0\x80\n", "0\t1\tRadio\t\xed\xa0\x80\n",
           "0\t1\tRadio\t\xf4\x90\x80\x80\n", "0\t.5\tRadio\tText\n",
           "0\t11\tRadio\tText\n", "nan\t2\tRadio\tText\n",
           "0\t2\tRadio\tText\n1\t3\tRadio\tOverlap\n",
           "0\t2\tRadio\t" + std::string(160, 'x') + "\n"}) {
    write(root / "captions/radio.captions", bad);
    EXPECT_THROW(static_cast<void>(prepareAudioContent(root, a)),
                 std::runtime_error)
        << bad;
  }
  EXPECT_EQ(captionScalars("Ёё — «текст»").size(), 12U);
}

TEST_F(AudioPersistence, NativeUnicodePathsAndMissingCaptionDiagnostics) {
  root /= std::filesystem::path(u8"Звуки и подписи");
  std::filesystem::create_directories(root / "audio");
  std::filesystem::create_directories(root / "captions");
  auto a = authoredAudio();
  a.cues.resize(1);
  std::filesystem::copy_file("resources/audio/radio.wav",
                             root / "audio/radio.wav");
  try {
    static_cast<void>(prepareAudioContent(root, a));
    FAIL();
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("radio-cue"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("radio.captions"),
              std::string::npos);
  }
  std::filesystem::copy_file("resources/captions/radio.captions",
                             root / "captions/radio.captions");
  EXPECT_EQ(prepareAudioContent(root, a).clip("radio").duration(), 10);
  root = root.parent_path();
}

TEST_F(AudioPersistence, FileDurationAndAggregateDecodeLimitsAreChecked) {
  std::filesystem::create_directory(root / "audio");
  auto a = authoredAudio();
  a.cues.resize(1);
  a.cues[0].caption.reset();
  a.cues[0].spatial = false;
  const auto path = root / "audio/radio.wav";
  {
    std::ofstream f(path, std::ios::binary);
    f.seekp(32 * 1024 * 1024);
    f.put(0);
  }
  EXPECT_THROW(static_cast<void>(prepareAudioContent(root, a)),
               std::runtime_error);
  const auto wave = [&](unsigned channels, unsigned frames) {
    std::string bytes(44 + channels * frames * 2, '\0');
    const auto number = [&](std::size_t offset, unsigned value,
                            unsigned width) {
      for (unsigned i = 0; i < width; ++i)
        bytes[offset + i] = static_cast<char>(value >> (8 * i));
    };
    bytes.replace(0, 4, "RIFF");
    bytes.replace(8, 8, "WAVEfmt ");
    bytes.replace(36, 4, "data");
    number(4, static_cast<unsigned>(bytes.size() - 8), 4);
    number(16, 16, 4);
    number(20, 1, 2);
    number(22, channels, 2);
    number(24, 48000, 4);
    number(28, channels * 96000, 4);
    number(32, channels * 2, 2);
    number(34, 16, 2);
    number(40, channels * frames * 2, 4);
    return bytes;
  };
  write(path, wave(1, 120 * 48000 + 1));
  try {
    static_cast<void>(prepareAudioContent(root, a));
    FAIL();
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("120 seconds"), std::string::npos);
  }
  const auto stereo = wave(2, 120 * 48000);
  for (const auto id : {"radio", "phone-ring", "footsteps"})
    write(root / "audio" / (std::string(id) + ".wav"), stereo);
  a.cues.push_back(
      {"ring", "phone-ring", {}, AudioCueKind::Ambience, false, false});
  a.cues.push_back(
      {"steps", "footsteps", {}, AudioCueKind::Ambience, false, false});
  try {
    static_cast<void>(prepareAudioContent(root, a));
    FAIL();
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("128 MiB"), std::string::npos);
  }
  a.cues.pop_back();
  a.cues.push_back(a.cues.front());
  EXPECT_EQ(prepareAudioContent(root, a).clips.size(), 2U);
}

namespace {
LevelAudio cueDefinitions() {
  auto a = authoredAudio();
  a.connections[0].door.reset();
  a.cues.push_back({"invitation-cue", "invitation", "invitation",
                    AudioCueKind::Dialogue, false, true});
  a.sources.push_back(
      {"invitation-source", "invitation-cue", {4, 0, 0}, 1, 1, 20, false});
  return a;
}
void audioFailure(const char* stage) {
#if defined(_WIN32)
  ASSERT_EQ(_putenv_s("NEAR_LAUGH_FORCE_AUDIO_FAILURE_STAGE", stage), 0);
#else
  ASSERT_EQ(setenv("NEAR_LAUGH_FORCE_AUDIO_FAILURE_STAGE", stage, 1), 0);
#endif
}
struct ClearAudioFailure {
  ~ClearAudioFailure() { audioFailure(""); }
};
}  // namespace

TEST(CueCoordinator, ForegroundStartBusyCancelAndCompleteAreExplicit) {
  const auto a = cueDefinitions();
  CueCoordinator cues(a, {}, prepareAudioContent("resources", a),
                      AudioOutput::Silent, 100);
  cues.autoplay();
  const auto radio_gain = cues.effectiveGain("radio-source");
  EXPECT_EQ(cues.start("phone-source"), CueStart::Started);
  const auto serial = cues.instance("phone-source");
  EXPECT_EQ(cues.start("phone-source"), CueStart::AlreadyActive);
  EXPECT_EQ(cues.instance("phone-source"), serial);
  EXPECT_EQ(cues.start("invitation-source"), CueStart::Busy);
  EXPECT_NE(cues.captions().foreground.text.find("вернёшься"),
            std::string_view::npos);
  EXPECT_FLOAT_EQ(cues.effectiveGain("radio-source"), radio_gain * .25F);
  cues.update(104.2);
  EXPECT_NE(cues.captions().foreground.text.find("не вернусь"),
            std::string_view::npos);
  EXPECT_TRUE(cues.cancel("phone-source"));
  EXPECT_TRUE(cues.captions().foreground.text.empty());
  EXPECT_EQ(cues.status("phone-source"), CueStatus::Cancelled);
  EXPECT_FLOAT_EQ(cues.effectiveGain("radio-source"), radio_gain);
  EXPECT_EQ(cues.start("invitation-source"), CueStart::Started);
  cues.update(111);
  EXPECT_EQ(cues.status("invitation-source"), CueStatus::Completed);
  EXPECT_TRUE(cues.captions().foreground.text.empty());
  cues.update(130);
  EXPECT_EQ(cues.status("phone-source"), CueStatus::Cancelled);
  EXPECT_EQ(cues.start("phone-source"), CueStart::Started);
  EXPECT_GT(cues.instance("phone-source"), serial);
  EXPECT_EQ(cues.offset("phone-source"), 0);
  EXPECT_EQ(cues.definitions(), a);
}

TEST(CueCoordinator,
     SilentAndMutedRunsPreserveEssentialTextAndFreezeSuspendedTime) {
  const auto a = cueDefinitions();
  for (auto output : {AudioOutput::Silent, AudioOutput::Offline}) {
    CueCoordinator cues(a, {}, prepareAudioContent("resources", a), output, 0);
    cues.mute(true);
    cues.autoplay();
    ASSERT_EQ(cues.start("phone-source"), CueStart::Started);
    cues.moveSource("phone-source", {100, 0, 0});
    std::vector<float> pcm(48000 * 2);
    cues.playback().render(pcm);
    cues.update(1);
    EXPECT_FALSE(cues.captions().foreground.text.empty());
    EXPECT_EQ(cues.effectiveGain("phone-source"), 0);
    const auto text = std::string(cues.captions().foreground.text);
    const auto serial = cues.instance("phone-source");
    cues.suspend(true, 1);
    cues.update(1001);
    EXPECT_EQ(cues.offset("phone-source"), 1);
    EXPECT_EQ(cues.captions().foreground.text, text);
    cues.suspend(false, 2001);
    cues.playback().render(pcm);
    cues.update(2002);
    EXPECT_EQ(cues.offset("phone-source"), 2);
    EXPECT_EQ(cues.instance("phone-source"), serial);
    cues.update(2020);
    EXPECT_EQ(cues.status("phone-source"), CueStatus::Completed);
    EXPECT_TRUE(cues.captions().foreground.text.empty());
    EXPECT_TRUE(
        cues.captions().ambience.text.empty());  // Loop caption never repeats.
    cues.suspend(true, 2020);
    cues.suspend(false, 3000);
    EXPECT_EQ(cues.status("phone-source"), CueStatus::Completed);
  }
}

TEST(CueCoordinator, AmbienceLaneUsesEffectiveGainThenDurableId) {
  auto a = cueDefinitions();
  a.sources.push_back(a.sources[0]);
  a.sources.back().id = "a-radio";
  a.sources.back().gain = .25F;
  a.cues[0].spatial = false;
  CueCoordinator cues(a, {}, prepareAudioContent("resources", a),
                      AudioOutput::Silent, 0);
  cues.autoplay();
  ASSERT_EQ(cues.start("phone-source"), CueStart::Started);
  EXPECT_FALSE(cues.captions().foreground.text.empty());
  EXPECT_FALSE(cues.captions().ambience.text.empty());
  EXPECT_GT(cues.effectiveGain("radio-source"), cues.effectiveGain("a-radio"));
}

TEST(CueCoordinator,
     CursorDriftRealignsLoopsWithoutReplayingCaptionsOrInstances) {
  const auto a = cueDefinitions();
  CueCoordinator cues(a, {}, prepareAudioContent("resources", a),
                      AudioOutput::Offline, 0);
  cues.autoplay();
  const auto serial = cues.instance("radio-source");
  cues.update(21.25);
  cues.update(21.251);
  cues.update(21.252);
  EXPECT_FALSE(
      cues.playback()
          .silent());  // The mixer has not consumed the pending seek yet.
  std::array<float, 960> pcm{};
  cues.playback().render(pcm);
  EXPECT_NEAR(cues.playback().cursor(0), 1.26, .02);
  cues.update(21.26);
  EXPECT_FALSE(cues.playback().silent());
  EXPECT_EQ(cues.instance("radio-source"), serial);
  EXPECT_TRUE(cues.captions().ambience.text.empty());
  ClearAudioFailure restore;
  audioFailure("seek");
  cues.update(22);
  cues.update(23);
  cues.update(24);
  EXPECT_TRUE(cues.playback().silent());
  EXPECT_FALSE(cues.playback().warning().empty());
  EXPECT_EQ(cues.status("radio-source"), CueStatus::Playing);
  EXPECT_EQ(cues.instance("radio-source"), serial);
}

TEST(CueCoordinator,
     InitializationVoiceAndDeviceLossFailuresRetainTheTimeline) {
  const auto a = cueDefinitions();
  ClearAudioFailure restore;
  for (const auto* stage :
       {"device-init", "after-engine", "voice", "device-loss"}) {
    audioFailure(stage);
    CueCoordinator cues(a, {}, prepareAudioContent("resources", a),
                        AudioOutput::Offline, 0);
    ASSERT_EQ(cues.start("phone-source"), CueStart::Started);
    cues.update(4);
    EXPECT_TRUE(cues.playback().silent());
    EXPECT_FALSE(cues.playback().warning().empty());
    EXPECT_EQ(cues.playback().voiceCount(), 0U);
    EXPECT_EQ(cues.status("phone-source"), CueStatus::Playing);
    EXPECT_FALSE(cues.captions().foreground.text.empty());
    const auto serial = cues.instance("phone-source");
    audioFailure("");
    cues.update(5);
    EXPECT_TRUE(cues.playback().silent());
    EXPECT_EQ(cues.instance("phone-source"), serial);
  }
}
