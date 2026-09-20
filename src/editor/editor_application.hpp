#ifndef EDITOR_EDITOR_APPLICATION_HPP
#define EDITOR_EDITOR_APPLICATION_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>

#include "core/animation/character_scene.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "editor/editor_audio_audition.hpp"
#include "editor/editor_camera.hpp"
#include "editor/editor_document.hpp"
#include "editor/editor_glfw_bridge.hpp"
#include "editor/editor_renderer.hpp"
#include "editor/editor_ui.hpp"
#if defined(NEAR_LAUGH_UI_AUTOMATION)
#include "editor/automation/engine_session.hpp"
#include "editor/automation/session.hpp"
#endif

class EditorApplication {
 public:
  EditorApplication(std::filesystem::path resource_root,
                    std::optional<std::filesystem::path> initial_level,
                    ValidationDiagnostics& diagnostics,
                    FrameCapture* capture = nullptr
#if defined(NEAR_LAUGH_UI_AUTOMATION)
                    , editor_automation::SessionController* session = nullptr
#endif
                    );
  ~EditorApplication();

  EditorApplication(const EditorApplication&) = delete;
  EditorApplication& operator=(const EditorApplication&) = delete;
  EditorApplication(EditorApplication&&) = delete;
  EditorApplication& operator=(EditorApplication&&) = delete;

  void run();
  void runSmoke(const std::filesystem::path& valid_level);
  void runCharacterSmoke(std::vector<std::string>& events,
                         FrameCapture& capture);
  void runHouseholdSmoke(std::vector<std::string>& events,
                         FrameCapture& capture,
                         const std::filesystem::path& capture_directory);
  [[nodiscard]] bool tick();

 private:
#if defined(NEAR_LAUGH_UI_AUTOMATION)
  friend struct EditorAutomationInputProbe;
#endif
  void updateNavigation(EditorUiCaptureIntent capture);
  void synchronizeDocumentResources();
  void updateAudition(double now, bool can_start = true);
  void updateCharacterPreview(double now, bool can_start);
  bool startCharacterPreview(const EditorCharacterPreviewRequest& request);
  bool startAudition(EditorObjectId source, double now,
                     AudioOutput output = AudioOutput::Device);
  void stopInspections();
  void launchPlay(const EditorLaunchRequest& launch);

  ValidationDiagnostics& validation_diagnostics_;
  std::filesystem::path resource_root_;
  std::shared_ptr<const CaptionFont> caption_font_;
  EditorAudioAudition audition_;
  EditorCharacterPreview character_preview_;
  std::optional<double> character_preview_time_;
#if defined(NEAR_LAUGH_UI_AUTOMATION)
  editor_automation::SessionController* session_{};
  std::string automation_preview_error_;
#endif
  Platform platform_{};
  Window window_;
#if defined(NEAR_LAUGH_UI_AUTOMATION)
  // Outlives the bridge/context. The stop guard below runs before backends.
  editor_automation::EngineSession automation_{};
#endif
  EditorGlfwBridge glfw_imgui_bridge_;
  EditorDocument document_{};
  EditorRenderer renderer_;
  EditorUi ui_{};
  EditorGameProcess game_process_{};
  EditorCamera camera_{};
  EditorFrameClock frame_clock_{};
  std::uint64_t rendered_document_revision_{};
  std::uint64_t rendered_object_revision_{};
  bool scene_resources_installed_{};
  std::vector<std::uint8_t> preview_point_light_enabled_{};
  std::vector<CharacterPose> initial_character_palettes_{};
  std::vector<CharacterPoseFrame> initial_character_frames_{};
  std::vector<std::shared_ptr<const CharacterAsset>>
      initial_character_assets_{};
  std::vector<EditorObjectId> initial_character_ids_{};
  bool character_resources_current_{};
#if defined(NEAR_LAUGH_UI_AUTOMATION)
  editor_automation::SessionAttachment automation_attachment_{automation_, session_};
#endif
};

#endif
