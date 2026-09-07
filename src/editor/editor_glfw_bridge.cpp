#include "editor/editor_glfw_bridge.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>

#include <stdexcept>

#include "core/platform/window.hpp"
#include "core/testing/test_controls.hpp"
#include "core/text/caption_font.hpp"

namespace {
bool createEditorContext() {
  IMGUI_CHECKVERSION();
  if (ImGui::CreateContext() == nullptr) {
    return false;
  }
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
  io.IniFilename = nullptr;
  ImGui::StyleColorsDark();
  recordLifecycleEvent("editor.imgui-context.created");
  return true;
}

bool initializeEditorGlfw(void* native_window) {
  const bool initialized = ImGui_ImplGlfw_InitForVulkan(
      static_cast<GLFWwindow*>(native_window), true);
  if (initialized) {
    recordLifecycleEvent("editor.imgui-glfw.created");
  }
  return initialized;
}

void shutdownEditorGlfw() noexcept {
  ImGui_ImplGlfw_Shutdown();
  recordLifecycleEvent("editor.imgui-glfw.destroyed");
}

void destroyEditorContext() noexcept {
  ImGui::DestroyContext();
  recordLifecycleEvent("editor.imgui-context.destroyed");
}
}  // namespace

EditorBridgeLifetime::EditorBridgeLifetime(void* native_window,
                                           EditorBridgeOperations operations)
    : operations_(operations) {
  if (operations_.create_context == nullptr ||
      operations_.initialize_glfw == nullptr ||
      operations_.shutdown_glfw == nullptr ||
      operations_.destroy_context == nullptr) {
    throw std::invalid_argument("Editor bridge operations must be complete");
  }
  context_created_ = operations_.create_context();
  if (!context_created_) {
    throw std::runtime_error("Dear ImGui context creation failed");
  }
  try {
    glfw_initialized_ = operations_.initialize_glfw(native_window);
    if (!glfw_initialized_) {
      throw std::runtime_error("Dear ImGui GLFW backend initialization failed");
    }
  } catch (...) {
    operations_.destroy_context();
    context_created_ = false;
    throw;
  }
}

EditorBridgeLifetime::~EditorBridgeLifetime() {
  if (glfw_initialized_) {
    operations_.shutdown_glfw();
  }
  if (context_created_) {
    operations_.destroy_context();
  }
}

EditorGlfwBridge::EditorGlfwBridge(Window& window,
                                   std::shared_ptr<const CaptionFont> font)
    : font_(std::move(font)),
      lifetime_(std::make_unique<EditorBridgeLifetime>(
          window.surfaceBridgeHandle(),
          EditorBridgeOperations{createEditorContext, initializeEditorGlfw,
                                 shutdownEditorGlfw, destroyEditorContext})) {
  if (font_) {
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    static const ImWchar ranges[]{0x20,   0xff,   0x400,  0x45f,  0x2013,
                                  0x2014, 0x2018, 0x2019, 0x201c, 0x201d,
                                  0x2026, 0x2026, 0x2116, 0x2116, 0};
    const auto bytes = font_->fontBytes();
    if (!ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
            const_cast<std::uint8_t*>(bytes.data()),
            static_cast<int>(bytes.size()), 18, &config, ranges))
      throw std::runtime_error(
          "Unable to install trusted Cyrillic editor font");
  }
}

EditorGlfwBridge::~EditorGlfwBridge() = default;

void EditorGlfwBridge::beginFrame() {
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void EditorGlfwBridge::postEmptyEvent() noexcept { glfwPostEmptyEvent(); }

EditorUiCaptureIntent EditorGlfwBridge::captureIntent() const noexcept {
  const ImGuiIO& io = ImGui::GetIO();
  return {io.WantCaptureKeyboard, io.WantCaptureMouse};
}
