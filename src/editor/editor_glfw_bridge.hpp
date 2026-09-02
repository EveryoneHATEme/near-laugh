#ifndef EDITOR_EDITOR_GLFW_BRIDGE_HPP
#define EDITOR_EDITOR_GLFW_BRIDGE_HPP

#include <memory>

#include "editor/editor_camera.hpp"

class Window;

struct EditorBridgeOperations {
  bool (*create_context)();
  bool (*initialize_glfw)(void* native_window);
  void (*shutdown_glfw)() noexcept;
  void (*destroy_context)() noexcept;
};

class EditorBridgeLifetime {
 public:
  EditorBridgeLifetime(void* native_window, EditorBridgeOperations operations);
  ~EditorBridgeLifetime();

  EditorBridgeLifetime(const EditorBridgeLifetime&) = delete;
  EditorBridgeLifetime& operator=(const EditorBridgeLifetime&) = delete;
  EditorBridgeLifetime(EditorBridgeLifetime&&) = delete;
  EditorBridgeLifetime& operator=(EditorBridgeLifetime&&) = delete;

 private:
  EditorBridgeOperations operations_{};
  bool context_created_{};
  bool glfw_initialized_{};
};

class EditorGlfwBridge {
 public:
  explicit EditorGlfwBridge(Window& window);
  ~EditorGlfwBridge();

  EditorGlfwBridge(const EditorGlfwBridge&) = delete;
  EditorGlfwBridge& operator=(const EditorGlfwBridge&) = delete;
  EditorGlfwBridge(EditorGlfwBridge&&) = delete;
  EditorGlfwBridge& operator=(EditorGlfwBridge&&) = delete;

  void beginFrame();
  static void postEmptyEvent() noexcept;
  [[nodiscard]] EditorUiCaptureIntent captureIntent() const noexcept;

 private:
  std::unique_ptr<EditorBridgeLifetime> lifetime_;
};

#endif
