#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "core/audio/transmission.hpp"
#include "core/gameplay/character_controller.hpp"
#include "core/gameplay/door_controller.hpp"
#include "core/player/player_controller.hpp"
#include "core/simulation/fixed_step.hpp"
#include "development/scripted_character_measurement.hpp"
#include "editor/editor_document.hpp"

namespace {
LevelDocument routeDocument(bool sound = false) {
  LevelDocument d;
  d.solids = {{{0, -.25F, 0},
               {10, .25F, 10},
               {255, 255, 255, 255},
               PrototypeSolidKind::Floor}};
  d.entries = {{"default", {{-4, 0, -4}, 0}}};
  d.default_entry = "default";
  d.characters.marks = {{"start", {0, 0, 0}, 0}, {"end", {0, 0, 2}, 180}};
  d.characters.actors = {{"actor", "test-mannequin", "start", {}, 1, {}, {}}};
  d.characters.routes = {{"route", "actor", {"end"}, "interact"},
                         {"return", "actor", {"start"}, {}}};
  if (sound) {
    d.audio.cues = {
        {"step", "character-footstep", {}, AudioCueKind::Ambience, false, true},
        {"touch", "character-interaction", "character-interaction",
         AudioCueKind::Essential, false, true}};
    d.audio.sources = {
        {"step", "step"}, {"touch", "touch"}, {"other", "touch"}};
    d.characters.actors[0].footstep_source = "step";
    d.characters.actors[0].interaction_source = "touch";
  }
  return d;
}
struct RouteRun {
  PrototypeLevel level;
  std::vector<std::shared_ptr<const CharacterAsset>> assets;
  CueCoordinator audio;
  PhysicsWorld physics;
  CharacterController controller;
  double now{};
  explicit RouteRun(const LevelDocument& d,
                    AudioOutput output = AudioOutput::Silent)
      : level(makePrototypeLevel(d)),
        assets(prepareCharacterAssets("resources", level.characters())),
        audio(level.audio(), level.doors(),
              prepareAudioContent("resources", level.audio()), output, 0),
        physics(level),
        controller(level.characters(), assets, physics, audio) {}
  void steps(int count, bool handoff = true) {
    for (int n = 0; n < count; ++n) {
      now += 1. / 60;
      audio.update(now);
      physics.advanceWorld(1.F / 60);
      controller.fixedStep(1.F / 60);
      if (handoff) controller.handoffAudio();
    }
  }
};
}  // namespace

TEST(CharacterRoutes, StartIsIdempotentAndRestartRetainsAcceptedPose) {
  {
    RouteRun run(routeDocument());
    EXPECT_EQ(run.controller.result(0).action, CharacterAction::Idle);
    EXPECT_EQ(run.controller.start("actor", "route"), CharacterStart::Started);
    const auto serial = run.controller.result(0).instance;
    run.steps(45);
    EXPECT_EQ(run.controller.start("actor", "route"),
              CharacterStart::AlreadyActive);
    EXPECT_EQ(run.controller.start("actor", "return"), CharacterStart::Busy);
    EXPECT_EQ(run.controller.result(0).instance, serial);
    const auto accepted = run.physics.actorState(0).feet_position;
    EXPECT_GT(accepted.z, .5F);
    EXPECT_TRUE(run.controller.cancel("actor"));
    EXPECT_FALSE(run.controller.cancel("actor"));
    run.steps(180);
    EXPECT_EQ(run.physics.actorState(0).feet_position, accepted);
    EXPECT_EQ(run.controller.result(0).action, CharacterAction::Canceled);
    EXPECT_EQ(run.controller.start("actor", "return"), CharacterStart::Started);
    EXPECT_EQ(run.physics.actorState(0).feet_position, accepted);
    EXPECT_EQ(run.controller.result(0).instance, serial + 1);
    run.steps(300);
    EXPECT_EQ(run.controller.result(0).action, CharacterAction::Completed);
    EXPECT_LE(std::abs(run.physics.actorState(0).feet_position.z), .02F);
    run.steps(300);
    EXPECT_EQ(run.controller.result(0).instance, serial + 1);
  }
  RouteRun fresh(routeDocument());
  EXPECT_EQ(fresh.physics.actorState(0).feet_position, WorldPosition{});
  EXPECT_EQ(fresh.controller.result(0).instance, 0U);
}

TEST(CharacterRoutes, InitialRouteStartsOnceAndFacesBeforeFinalInteraction) {
  auto d = routeDocument();
  d.characters.actors[0].initial_route = "route";
  RouteRun run(d);
  EXPECT_EQ(run.controller.result(0).instance, 1U);
  bool turned = false, interacted = false;
  for (int step = 0; step < 500; ++step) {
    run.steps(1);
    const auto state = run.physics.actorState(0);
    if (run.controller.result(0).action == CharacterAction::Turning &&
        state.feet_position.z > 1) {
      turned = true;
      EXPECT_LE(std::abs(state.feet_position.z - 2), .02F);
      EXPECT_GE(state.yaw_degrees, 0);  // exact 180 tie turns positively
      EXPECT_EQ(run.controller.playback(0).clip(), "idle");
    }
    if (run.controller.result(0).action == CharacterAction::Interacting) {
      interacted = true;
      EXPECT_LE(std::abs(state.feet_position.z - 2), .02F);
      EXPECT_LE(std::abs(state.yaw_degrees - 180), 1);
    }
    const auto& frame = run.controller.presentation()[0];
    EXPECT_EQ(frame.position[2], state.feet_position.z);
    EXPECT_EQ(frame.yaw_degrees, state.yaw_degrees);
  }
  EXPECT_TRUE(turned);
  EXPECT_TRUE(interacted);
  EXPECT_EQ(run.controller.result(0).action, CharacterAction::Completed);
  EXPECT_EQ(run.controller.result(0).instance, 1U);
}

TEST(CharacterRoutes, HeadingAndRepeatedMarksKeepOrderedStandingTurns) {
  auto d = routeDocument();
  d.characters.marks[0].yaw_degrees = 180;
  d.characters.marks.push_back({"corner", {0, 0, 0}, 90});
  d.characters.routes[0].marks = {"corner", "corner", "end"};
  RouteRun run(d);
  (void)run.controller.start("actor", "route");
  run.steps(1);
  EXPECT_NEAR(run.physics.actorState(0).yaw_degrees, 178, .0001F);
  EXPECT_EQ(run.physics.actorState(0).feet_position, WorldPosition{});
  run.steps(44);
  EXPECT_NEAR(run.physics.actorState(0).yaw_degrees, 90, .0001F);
  run.steps(1);
  EXPECT_EQ(run.controller.result(0).mark, "end");
  EXPECT_EQ(run.controller.result(0).walked_distance, 0);
  run.steps(500);
  EXPECT_EQ(run.controller.result(0).action, CharacterAction::Completed);
}

TEST(CharacterRoutes,
     BlockedTravelFreezesPhaseAndCancelDiscardsPendingContacts) {
  {
    auto d = routeDocument(true);
    d.solids.push_back({{0, 1, 1.5F},
                        {1, 1, .01F},
                        {255, 255, 255, 255},
                        PrototypeSolidKind::Boundary});
    RouteRun run(d);
    (void)run.controller.start("actor", "route");
    run.steps(150);
    EXPECT_EQ(run.controller.result(0).action, CharacterAction::Blocked);
    EXPECT_EQ(run.controller.result(0).obstruction,
              PhysicsActorObstruction::Static);
    EXPECT_EQ(run.controller.playback(0).clip(), "idle");
    const auto distance = run.controller.result(0).walked_distance;
    const auto contacts = run.controller.result(0).contacts;
    const auto feet = run.physics.actorState(0).feet_position;
    EXPECT_GT(contacts, 0U);
    run.steps(180);
    EXPECT_EQ(run.controller.result(0).walked_distance, distance);
    EXPECT_EQ(run.controller.result(0).contacts, contacts);
    EXPECT_EQ(run.physics.actorState(0).feet_position, feet);
    EXPECT_EQ(run.audio.instance("touch"), 0U);
    EXPECT_TRUE(run.controller.cancel("actor"));
    run.steps(100);
    EXPECT_EQ(run.controller.result(0).action, CharacterAction::Canceled);
    EXPECT_EQ(run.physics.actorState(0).feet_position, feet);
  }
  RouteRun pending(routeDocument(true));
  (void)pending.controller.start("actor", "route");
  pending.steps(45, false);
  EXPECT_GT(pending.controller.result(0).contacts, 0U);
  EXPECT_EQ(pending.audio.instance("step"), 0U);
  (void)pending.controller.cancel("actor");
  pending.controller.handoffAudio();
  EXPECT_EQ(pending.audio.instance("step"), 0U);
}

TEST(CharacterRoutes, DoorClearanceResumesSavedDistancePhaseWithoutJump) {
  auto d = routeDocument();
  DoorDefinition door;
  door.id = "door";
  door.hinge_position = {-1, .02F, 1.5F};
  door.width = 2;
  door.open_angle_degrees = -90;
  d.doors = {door};
  RouteRun run(d);
  (void)run.controller.start("actor", "route");
  run.steps(160);
  EXPECT_EQ(run.controller.result(0).obstruction,
            PhysicsActorObstruction::Door);
  const auto before = run.controller.result(0).walked_distance;
  const auto pose = run.physics.actorState(0).feet_position;
  const auto opened = run.physics.advanceDoor(0, -90);
  EXPECT_FALSE(opened.obstructed) << "angle=" << opened.angle;
  run.steps(1);
  EXPECT_GT(run.controller.result(0).walked_distance, before);
  EXPECT_LE(run.physics.actorState(0).feet_position.z - pose.z,
            1.F / 60 + .0001F);
  const auto phase = std::fmod(run.controller.result(0).walked_distance /
                                   test_mannequin_catalog.walk_cycle_distance_m,
                               1.);
  EXPECT_NEAR(run.controller.playback(0).time(),
              phase * characterClip(*run.assets[0], "walk").duration, 1e-8);
  run.steps(400);
  EXPECT_EQ(run.controller.result(0).action, CharacterAction::Completed);
}

TEST(CharacterRoutes,
     MatchingHorizontalCoordinatesCannotSkipUnreachableHeight) {
  auto d = routeDocument();
  d.characters.marks[1].feet_position = {0, 3, 0};
  d.solids.push_back({{0, 2.9F, 0},
                      {2, .1F, 2},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::Floor});
  RouteRun run(d);
  (void)run.controller.start("actor", "route");
  run.steps(180);
  EXPECT_EQ(run.controller.result(0).action, CharacterAction::Blocked);
  EXPECT_EQ(run.controller.result(0).obstruction,
            PhysicsActorObstruction::Support);
  EXPECT_EQ(run.physics.actorState(0).feet_position, WorldPosition{});
  EXPECT_EQ(run.controller.result(0).contacts, 0U);
  EXPECT_EQ(run.controller.playback(0).clip(), "idle");
}

TEST(CharacterRoutes,
     BusyMarkerHoldsThenStartsOneOwnedInstanceAndCancelsLocally) {
  auto d = routeDocument(true);
  d.characters.routes[0].marks = {"start"};
  for (bool muted : {false, true}) {
    RouteRun run(d);
    run.audio.mute(muted);
    (void)run.controller.start("actor", "route");
    while (run.controller.playback(0).clip() != "interact" ||
           run.controller.playback(0).time() < .5)
      run.steps(1);
    EXPECT_EQ(run.audio.start("other"), CueStart::Started);
    const auto other = run.audio.instance("other");
    run.steps(30);
    EXPECT_TRUE(run.controller.result(0).audio_contention);
    const auto marker = characterClip(*run.assets[0], "interact").duration *
                        test_mannequin_catalog.interaction_phase;
    EXPECT_NEAR(run.controller.playback(0).time(), marker, 1e-9);
    EXPECT_EQ(run.audio.instance("touch"), 0U);
    run.steps(40);
    EXPECT_FALSE(run.controller.result(0).audio_contention);
    EXPECT_EQ(run.audio.status("touch"), CueStatus::Playing);
    const auto owned = run.audio.instance("touch");
    EXPECT_GT(owned, other);
    EXPECT_GT(run.controller.playback(0).time(), marker);
    run.steps(5);
    EXPECT_EQ(run.audio.instance("touch"), owned);
    EXPECT_TRUE(run.controller.cancel("actor"));
    EXPECT_EQ(run.audio.status("touch"), CueStatus::Cancelled);
    EXPECT_EQ(run.audio.instance("other"), other);
    EXPECT_TRUE(run.controller.ownsSource("touch"));
    EXPECT_FALSE(run.controller.ownsSource("other"));
  }
}

TEST(CharacterRoutes, CancelDuringAudioContentionLeavesOtherCuePlaying) {
  auto d = routeDocument(true);
  d.characters.routes[0].marks = {"start"};
  RouteRun run(d);
  (void)run.controller.start("actor", "route");
  run.steps(35);
  (void)run.audio.start("other");
  run.steps(25);
  EXPECT_TRUE(run.controller.result(0).audio_contention);
  (void)run.controller.cancel("actor");
  run.controller.handoffAudio();
  EXPECT_EQ(run.audio.status("other"), CueStatus::Playing);
  EXPECT_EQ(run.audio.instance("touch"), 0U);
}

TEST(CharacterContent, OnlySelectedAssetsAreRequiredAndModelsAreShared) {
  EXPECT_TRUE(prepareCharacterAssets("missing-assets", {}).empty());
  auto d = routeDocument();
  EXPECT_THROW((void)prepareCharacterAssets("missing-assets", d.characters),
               std::runtime_error);
  d.characters.actors.push_back(
      {"second", "test-mannequin", "end", {}, 1, {}, {}});
  const auto assets = prepareCharacterAssets("resources", d.characters);
  ASSERT_EQ(assets.size(), 2U);
  EXPECT_EQ(assets[0], assets[1]);
}

TEST(CharacterContent, ApprovedCueDurationsAndSilenceArePreflightConstraints) {
  auto d = routeDocument(true);
  auto content = prepareAudioContent("resources", d.audio);
  EXPECT_NO_THROW(validateCharacterAudio(content, d.audio, d.characters));
  const auto original = content;
  content.clips[0].samples.resize(7201);
  EXPECT_THROW(validateCharacterAudio(content, d.audio, d.characters),
               std::runtime_error);
  content = original;
  content.clips[1].samples[12000] = .1F;
  EXPECT_THROW(validateCharacterAudio(content, d.audio, d.characters),
               std::runtime_error);
  content = original;
  content.clips[1].samples.resize(12000);
  EXPECT_THROW(validateCharacterAudio(content, d.audio, d.characters),
               std::runtime_error);
  content = original;
  content.captions[0].segments[0].end = .25;
  EXPECT_THROW(validateCharacterAudio(content, d.audio, d.characters),
               std::runtime_error);
}

TEST(CharacterRoutes,
     PackagedRoutesClimbStairsWaitForDoorAndFinishAtAcceptedMark) {
  for (const auto name : {"scripted-characters", "scripted-characters-four"}) {
    const auto loaded =
        loadLevelDocument(std::filesystem::path("resources/levels") /
                          (std::string(name) + ".level.json"));
    ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
    RouteRun run(*loaded.document);
    DoorController doors(run.level.doors());
    bool blocked_door = false, on_stairs = false;
    for (int step = 0; step < 1800; ++step) {
      run.steps(1, false);
      const auto& result = run.controller.result(0);
      if (run.physics.actorState(0).feet_position.y > .15F) on_stairs = true;
      if (!blocked_door &&
          result.obstruction == PhysicsActorObstruction::Door) {
        blocked_door = true;
        EXPECT_EQ(run.audio.instance("walker-touch"), 0U);
        (void)doors.act(0, DoorAction::Interact, {0, 2, 4});
      }
      doors.fixedStep(1.F / 60, run.physics);
      run.audio.acceptedDoors(std::array<float, 1>{doors.state(0).angle});
      run.controller.handoffAudio();
    }
    EXPECT_TRUE(on_stairs);
    EXPECT_TRUE(blocked_door);
    EXPECT_EQ(run.controller.result(0).action, CharacterAction::Completed);
    EXPECT_GT(run.audio.instance("walker-touch"), 0U);
    const auto pose = run.physics.actorState(0);
    EXPECT_NEAR(pose.feet_position.y, .6F, .02F);
    EXPECT_NEAR(pose.feet_position.z, 5.3F, .02F);
    EXPECT_NEAR(pose.yaw_degrees, 90.F, 1.F);
    if (run.physics.actorCount() == 4) {
      EXPECT_EQ(run.controller.result(1).action, CharacterAction::Completed);
      EXPECT_EQ(run.controller.result(2).obstruction,
                PhysicsActorObstruction::Actor);
      EXPECT_EQ(run.controller.result(3).obstruction,
                PhysicsActorObstruction::Actor);
    }
  }
}

TEST(CharacterRoutes, FixedBatchesCapTravelAndEmitAtMostOneContactPerActor) {
  std::array<double, 3> distances;
  std::array<std::uint64_t, 3> contacts;
  for (int mode = 0; mode < 3; ++mode) {
    auto d = routeDocument(true);
    d.characters.marks[1].feet_position.z = 8;
    d.characters.actors[0].speed = 1.5F;
    RouteRun run(d);
    (void)run.controller.start("actor", "route");
    FixedStepAccumulator clock;
    for (int sample = 0; sample < (mode == 0 ? 240 : 40); ++sample) {
      const auto batch = clock.advance(mode == 0   ? 1. / 60
                                       : mode == 1 ? .1
                                                   : 3.);
      const auto before = run.controller.result(0);
      run.steps(batch.complete_steps, false);
      EXPECT_LE(run.controller.result(0).contacts - before.contacts, 1U);
      EXPECT_LE(
          run.controller.result(0).walked_distance - before.walked_distance,
          .150001);
      run.controller.handoffAudio();
      const auto serial = run.audio.instance("step");
      run.controller.handoffAudio();
      EXPECT_EQ(run.audio.instance("step"), serial);
    }
    distances[mode] = run.controller.result(0).walked_distance;
    contacts[mode] = run.controller.result(0).contacts;
  }
  EXPECT_EQ(distances[0], distances[1]);
  EXPECT_EQ(distances[1], distances[2]);
  EXPECT_EQ(contacts[0], contacts[1]);
  EXPECT_EQ(contacts[1], contacts[2]);
}

TEST(CharacterRoutes, InactiveJumpEdgeCannotReplayWhenSimulationResumes) {
  RouteRun run(routeDocument());
  PlayerController player(run.physics, 0);
  for (int i = 0; i < 10; ++i) {
    run.physics.advanceWorld(1.F / 60);
    player.fixedStep(1.F / 60);
  }
  PlayerActionSnapshot input;
  input.jump = true;
  player.sampleInput(input, true);
  player.sampleInput(input, false);
  player.sampleInput(input, true);
  run.physics.advanceWorld(1.F / 60);
  player.fixedStep(1.F / 60);
  EXPECT_LT(run.physics.characterState().linear_velocity.y, .1F);
}

TEST(CharacterContent, EditorUnrelatedEditUndoAndSavePreserveRoutes) {
  EditorDocument editor;
  ASSERT_TRUE(
      editor.open("resources/levels/scripted-characters-four.level.json"));
  const auto characters = editor.document()->characters;
  const auto id = editor.solidIds().front();
  auto floor = editor.document()->solids.front();
  floor.color[0] = 150;
  ASSERT_TRUE(editor.replaceObject(id, floor));
  EXPECT_EQ(editor.document()->characters, characters);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.document()->characters, characters);
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(editor.document()->characters, characters);
  const auto path = std::filesystem::path(
      "build/scripted-character-editor-roundtrip.level.json");
  ASSERT_TRUE(editor.saveAs(path));
  const auto roundtrip = loadLevelDocument(path);
  ASSERT_TRUE(roundtrip);
  EXPECT_EQ(roundtrip.document->characters, characters);
}

TEST(CharacterRoutes,
     AcceptedAudioPlacementAndDeviceLossPreserveActionIdentity) {
  std::array<std::uint64_t, 4> contacts{}, instances{};
  for (int mode = 0; mode < 4; ++mode) {
    auto d = routeDocument(true);
    // Authored source location deliberately differs from every accepted pose.
    d.audio.sources[0].position = {9, 0, 9};
    RouteRun run(d, mode == 2 ? AudioOutput::Silent : AudioOutput::Offline);
    run.audio.mute(mode == 1);
    const WorldPosition listener{-2, 1, 0};
    run.audio.listener(listener, {0, 0, 1});
    (void)run.controller.start("actor", "route");
    std::array<float, 1600> samples{};
    double energy{};
    bool caption_seen = false;
    for (int step = 0; step < 400; ++step) {
      run.steps(1);
      if (mode != 1 && run.audio.status("touch") != CueStatus::Playing) {
        const auto expected = audioDistanceGain(
            run.physics.actorState(0).feet_position, listener, 1, 20);
        EXPECT_NEAR(run.audio.effectiveGain("step"), expected, 1e-6);
      }
      caption_seen |= !run.audio.captions().foreground.text.empty();
      if (mode != 2 && !(mode == 3 && step >= 120)) {
        run.audio.playback().render(samples);
        for (float value : samples) energy += value * value;
      }
      if (mode == 3 && step == 119)
        run.audio.playback().silentFallback(
            "Injected character acceptance device loss");
    }
    EXPECT_TRUE(caption_seen);
    EXPECT_EQ(run.controller.result(0).action, CharacterAction::Completed);
    contacts[mode] = run.controller.result(0).contacts;
    instances[mode] = run.audio.instance("touch");
    if (mode == 0 || mode == 3) EXPECT_GT(energy, .0001);
    if (mode == 1 || mode == 2) EXPECT_EQ(energy, 0);
  }
  for (int mode = 1; mode < 4; ++mode) {
    EXPECT_EQ(contacts[0], contacts[mode]);
    EXPECT_EQ(instances[0], instances[mode]);
  }
}

TEST(CharacterRoutes,
     MeasurementLanesTraverseRepeatedlyWithUnchangedLightScene) {
  const auto baseline = scriptedCharacterMeasurement("resources", 0);
  for (unsigned count : {1U, 4U}) {
    auto d = scriptedCharacterMeasurement("resources", count);
    EXPECT_EQ(d.solids, baseline.solids);
    EXPECT_EQ(d.props, baseline.props);
    EXPECT_EQ(d.doors, baseline.doors);
    EXPECT_EQ(d.entries, baseline.entries);
    EXPECT_EQ(d.environment_light, baseline.environment_light);
    RouteRun run(d);
    std::array<int, 4> completed{};
    for (int step = 0; step < 2400; ++step) {
      run.steps(1);
      for (unsigned i = 0; i < count; ++i) {
        const auto& result = run.controller.result(i);
        ASSERT_NE(result.action, CharacterAction::Blocked) << result.actor;
        if (result.action != CharacterAction::Completed) continue;
        ++completed[i];
        // Both the previous leg and this leg may stop within 0.02 m.
        EXPECT_GE(result.walked_distance, .75 - 2 * .02);
        const auto& id = d.characters.actors[i].id;
        (void)run.controller.start(
            id, id + (result.route.ends_with("-out") ? "-back" : "-out"));
      }
    }
    for (unsigned i = 0; i < count; ++i) EXPECT_GE(completed[i], 4);
  }
}

TEST(CharacterRoutes,
     ContactHandoffMeasuresBatchDelayAndOfflineOnsetWithoutDrift) {
  auto d = routeDocument(true);
  d.characters.marks[1].feet_position.z = 8;
  d.characters.routes[0].final_clip.reset();
  d.characters.actors[0].speed = 1.5F;
  RouteRun run(d, AudioOutput::Offline);
  (void)run.controller.start("actor", "route");
  std::filesystem::create_directories("build");
  std::ofstream trace("build/scripted-character-contact-timing.csv");
  trace << "contact,simulation_seconds,handoff_seconds,batch_delay_ms,offline_"
           "onset_ms\n";
  std::array<float, 9600> pcm{};
  double maximum_delay{}, maximum_onset{};
  std::uint64_t observed{};
  for (int batch = 0; batch < 50; ++batch) {
    double contact_time = -1;
    for (int step = 0; step < 6; ++step) {
      const auto before = run.controller.result(0).contacts;
      run.steps(1, false);
      if (run.controller.result(0).contacts != before) contact_time = run.now;
    }
    run.controller.handoffAudio();
    run.audio.playback().render(pcm);
    if (contact_time < 0) continue;
    const auto onset = std::find_if(pcm.begin(), pcm.end(), [](float value) {
      return std::abs(value) > .00001F;
    });
    ASSERT_NE(onset, pcm.end());
    const double onset_ms = (std::distance(pcm.begin(), onset) / 2) / 48.;
    const double delay_ms = (run.now - contact_time) * 1000;
    maximum_delay = std::max(maximum_delay, delay_ms);
    maximum_onset = std::max(maximum_onset, onset_ms);
    trace << ++observed << ',' << contact_time << ',' << run.now << ','
          << delay_ms << ',' << onset_ms << '\n';
    EXPECT_LE(delay_ms + onset_ms, 100);
  }
  EXPECT_GT(observed, 5U);
  EXPECT_EQ(observed, run.controller.result(0).contacts);
  RecordProperty("maximum_batch_delay_ms", std::to_string(maximum_delay));
  RecordProperty("maximum_offline_onset_ms", std::to_string(maximum_onset));
  std::cout << "Contact timing: " << observed
            << " contacts, maximum batch delay " << maximum_delay
            << " ms, maximum offline onset " << maximum_onset
            << " ms; hardware latency unavailable\n";
}
