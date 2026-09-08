#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/animation/character_animation.hpp"
#include "core/animation/character_catalog.hpp"
#include "core/development/frame_capture.hpp"
#include "core/development/frame_timings.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/renderer.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/text/caption_font.hpp"
#include "core/world/door.hpp"
#include "core/world/light_switch.hpp"
#include "core/world/prototype_level.hpp"
#include "development/character_fixture.hpp"
#include "launcher/executable_path.hpp"

namespace {
constexpr std::array<std::string_view, 3> clips{"idle", "walk", "interact"};
unsigned instanceCount(std::string_view text) {
  if (text == "0") return 0;
  if (text == "1") return 1;
  if (text == "4") return 4;
  throw std::invalid_argument("Character count must be 0, 1 or 4");
}

// The fixture owns all clocks and poses. Renderer recovery cannot restart them.
class Viewer {
 public:
  Viewer(const std::filesystem::path& root, unsigned count, bool measure,
         FrameTimings* timings, FrameCapture* capture, ValidationDiagnostics& diagnostics)
      : window_(platform_, measure ? 1920 : 1280, measure ? 1080 : 720,
                "Character animation: F5 clip, P pause, A/D seek, Space restart, M one/four, E view"),
        root_(root), count_(count), measurement_(measure), timings_(timings), capture_(capture),
        diagnostics_(diagnostics),
        level_(measure ? loadPrototypeLevel(root / "levels/interior-lighting-capacity.level.json")
                       : makePrototypeLevel(character_fixture::controlScene())),
        enabled_(initialPointLightEnabled(level_.environmentLight())) {
    if (count_ != 0) {
      asset_ = loadCharacterAsset(root_ / test_mannequin_catalog.resource);
      const unsigned playback_count = measurement_ ? count_ : 4;
      for (unsigned i = 0; i < playback_count; ++i) {
        playback_.emplace_back(asset_, clips[i % clips.size()]);
        playback_.back().seek(i * .23);
      }
    }
    if (measurement_) window_.useFullscreenMode(1920, 1080, 60);
    else font_ = std::make_shared<const CaptionFont>(root_);
    for (const auto& door : level_.doors()) {
      const auto boxes = doorPresentationBoxes(door, doorInitialAngle(door),
                                                door.initially_locked);
      boxes_.insert(boxes_.end(), boxes.begin(), boxes.end());
    }
    prepareRenderer();
  }

  void run(bool check) {
    using Clock = FrameTimings::Clock;
    auto previous = Clock::now();
    unsigned frames{};
    bool resumed{};
    std::cout << "Validation " << (renderer_->validationEnabled() ? "enabled" : "disabled")
              << "; FIFO; characters " << count_
              << (measurement_ ? "; 1920x1080/60 Hz; warmup 10 s; sample 60 s\n"
                               : "; interactive clip inspection\n");
    while (true) {
      const auto now = Clock::now();
      if (measurement_ && (timings_->elapsedSeconds(now) >= 70 || (check && frames >= 40))) break;
      if (timings_) timings_->beginFrame(now);
      window_.pollEvents();
      if (window_.shouldClose()) {
        if (measurement_) throw std::runtime_error("Measurement window closed before completion");
        break;
      }
      const auto extent = window_.framebufferExtent();
      if (extent.isZero()) {
        if (measurement_) throw std::runtime_error("Measurement interrupted by minimization");
        previous_keys_ = window_.input().keys;
        window_.waitEvents();
        previous_keys_ = window_.input().keys;
        previous = Clock::now();
        resumed = true;
        continue;
      }
      if (!measurement_) controls();
      const double elapsed = resumed ? 0 : std::max(0., std::chrono::duration<double>(now - previous).count());
      resumed = false;
      previous = now;
      for (auto& playback : playback_) (void)playback.advance(elapsed);
      // The fourth character exercises clip transitions in timing runs.
      if (measurement_ && count_ == 4) {
        const auto next = static_cast<unsigned>(timings_->elapsedSeconds(now) / 2) % clips.size();
        if (playback_[3].clip() != clips[next]) playback_[3].selectClip(clips[next]);
      }
      updatePoses();
      const auto eye = measurement_ ? WorldPosition{-3.15F, 5.65F, 5.85F}
          : view_ == 0 ? WorldPosition{0, 2.2F, 5}
          : view_ == 1 ? WorldPosition{4, 2, 0} : WorldPosition{0, 3, -5};
      const auto target = measurement_ ? WorldPosition{-3.15F, 3.8F, 2.7F} : WorldPosition{0, .9F, 0};
      FrameRequest frame{extent, window_.consumeFramebufferResize(),
                         character_fixture::camera(eye, target, static_cast<float>(extent.width) / extent.height),
                         flashlight_ ? character_fixture::flashlight(eye, target) : SpotLightFrame{}, enabled_};
      frame.opaque_boxes = boxes_;
      frame.characters = frames_;
      std::string status;
      if (!measurement_) {
        std::ostringstream text;
        text << playback_[0].clip() << "  " << std::fixed << std::setprecision(2)
             << playback_[0].time() << " s  " << (playback_[0].paused() ? "paused" : "playing")
             << "  " << count_ << " characters";
        status = text.str();
        frame.captions = {{"", status}, {"", "F5 clip | P pause | A/D seek | Space restart | M one/four | E view | R flashlight | Escape close"}};
      }
      if (timings_) timings_->current().action = "characters-" + std::to_string(count_);
      if (check && frames == 20) renderer_->requestSwapchainRecreation();
      if (check && frames == 30) capture_->requested = true;
      const auto outcome = renderer_->renderFrame(frame);
      if (!runtimeContinuesAfter(outcome)) throw std::runtime_error("Unexpected renderer outcome");
      if (timings_) {
        if (timings_->current().submitted && (extent.width != 1920 || extent.height != 1080))
          throw std::runtime_error("Measurement requires a 1920x1080 framebuffer");
        timings_->endFrame(Clock::now());
      }
      ++frames;
    }
    std::cout << "Captured " << frames << " frames\n";
  }

 private:
  void prepareRenderer() {
    RendererResources resources{root_ / "shaders/prototype_scene_vertex.spv",
                                root_ / "shaders/prototype_scene_fragment.spv", root_};
    resources.caption_font = font_;
    resources.timings = timings_;
    resources.capture = capture_;
    for (unsigned i = 0; i < count_; ++i) resources.characters.push_back({i + 1, asset_});
    // Count changes are explicit development scene replacement, retaining pose owners.
    renderer_.reset();
    renderer_ = std::make_unique<Renderer>(window_, window_.framebufferExtent(), level_,
                                         std::move(resources), diagnostics_);
  }
  void updatePoses() {
    frames_.clear();
    for (unsigned i = 0; i < count_; ++i) {
      poses_[i] = playback_[i].pose();
      const float x = count_ == 1 ? 0 : (static_cast<float>(i % 2) - .5F) * (measurement_ ? 2.1F : 1.6F);
      const float z = count_ == 1 ? 0 : -static_cast<float>(i / 2) * 1.5F;
      frames_.push_back({i + 1, asset_->skeleton_identity, poses_[i],
                        measurement_ ? std::array<float, 3>{-3.15F + x, 3, 3.5F + z}
                                     : std::array<float, 3>{x, 0, z},
                        i == 3 ? 35.F : 0.F});
    }
  }
  void controls() {
    const auto& input = window_.input();
    const auto pressed = [&](PhysicalKey key) {
      return input.isKeyDown(key) && !previous_keys_[static_cast<std::size_t>(key)];
    };
    if (pressed(PhysicalKey::Escape)) window_.requestClose();
    if (pressed(PhysicalKey::P)) {
      const bool pause = !playback_[0].paused();
      for (auto& playback : playback_) playback.setPaused(pause);
    }
    if (pressed(PhysicalKey::F5)) {
      clip_ = (clip_ + 1) % clips.size();
      for (unsigned i = 0; i < playback_.size(); ++i)
        playback_[i].selectClip(clips[(clip_ + i) % clips.size()]);
    }
    if (pressed(PhysicalKey::Space)) for (auto& playback : playback_) playback.restart();
    if (pressed(PhysicalKey::A) || pressed(PhysicalKey::D)) {
      const double step = pressed(PhysicalKey::D) ? .1 : -.1;
      for (auto& playback : playback_) {
        playback.setPaused(true);
        playback.seek(std::max(0., playback.time() + step));
      }
    }
    if (pressed(PhysicalKey::E)) view_ = (view_ + 1) % 3;
    if (pressed(PhysicalKey::R)) flashlight_ = !flashlight_;
    if (pressed(PhysicalKey::M)) {
      count_ = count_ == 1 ? 4 : 1;
      prepareRenderer();
    }
    previous_keys_ = input.keys;
  }
  Platform platform_;
  Window window_;
  std::filesystem::path root_;
  unsigned count_{};
  bool measurement_{};
  FrameTimings* timings_{};
  FrameCapture* capture_{};
  ValidationDiagnostics& diagnostics_;
  PrototypeLevel level_;
  std::vector<std::uint8_t> enabled_;
  std::vector<OpaqueBoxFrame> boxes_;
  std::shared_ptr<const CharacterAsset> asset_;
  std::shared_ptr<const CaptionFont> font_;
  std::vector<CharacterPlayback> playback_;
  std::array<CharacterPose, 4> poses_;
  std::vector<CharacterPoseFrame> frames_;
  std::array<bool, static_cast<std::size_t>(PhysicalKey::Count)> previous_keys_{};
  unsigned clip_{}, view_{};
  bool flashlight_{};
  std::unique_ptr<Renderer> renderer_;
};

template <class Char> int run(int argc, Char** argv) {
  try {
    const auto root = launcher::executableResourceRoot();
    const auto mode = argc > 1 ? std::filesystem::path(argv[1]).string() : "";
    if (argc == 2 && mode == "--preflight") {
      const auto asset = loadCharacterAsset(root / test_mannequin_catalog.resource);
      for (const auto clip : clips) {
        const auto pose = evaluateCharacterPose(*asset, sampleCharacterPose(*asset, clip, .5));
        (void)deformCharacter(*asset, pose);
      }
      std::cout << "Prepared character resources loaded from executable-relative layout\n";
      return 0;
    }
    const bool measure = mode == "--measure" || mode == "--check";
    if ((!measure && argc != 1) || (measure && argc != 4))
      throw std::invalid_argument("Usage: character_animation_viewer [--preflight | --measure <0|1|4> <new.csv> | --check <0|1|4> <new.csv>]");
    const unsigned count = measure ? instanceCount(std::filesystem::path(argv[2]).string()) : 1;
    const auto output = measure ? std::filesystem::absolute(std::filesystem::path(argv[3])) : std::filesystem::path{};
    if (measure && std::filesystem::exists(output)) throw std::invalid_argument("Choose a fresh timing output path");
    const bool check = mode == "--check";
    auto capture_output = output;
    capture_output.replace_extension(".ppm");
    if (check && std::filesystem::exists(capture_output))
      throw std::invalid_argument("Choose a fresh check capture output path");
    FrameCapture capture;
    std::unique_ptr<FrameTimings> timings = measure ? std::make_unique<FrameTimings>() : nullptr;
    ValidationDiagnostics diagnostics;
    try {
      Viewer(root, count, measure, timings.get(), check ? &capture : nullptr, diagnostics).run(check);
    } catch (...) {
      if (timings) timings->writeCsv(output);
      throw;
    }
    if (timings) timings->writeCsv(output);  // GPU teardown drained pending queries.
    if (check) capture.writePpm(capture_output);
    if (diagnostics.errorCount()) throw std::runtime_error("Vulkan validation errors including teardown");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
}  // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return run(argc, argv); }
#else
int main(int argc, char** argv) { return run(argc, argv); }
#endif
