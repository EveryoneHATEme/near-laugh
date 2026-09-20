#include <chrono>
#include <cstdio>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include <io.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "editor/automation/channel.hpp"
#include "editor/automation/session.hpp"
#include "editor/editor_application.hpp"

int main(int argc, char** argv) {
  if (argc != 2 || std::string_view(argv[1]) != "--automation-pipe") {
    std::cerr << "This editor requires its private automation host.\n";
    return 2;
  }
  try {
    editor_automation::Channel channel;
    // Channel already owns its duplicated protocol handle. Ordinary renderer
    // diagnostics (including C stdio) must never enter that JSON stream.
    if (_dup2(_fileno(stderr), _fileno(stdout)) != 0 ||
        !SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE)))
      throw std::runtime_error("Cannot isolate automation diagnostic output");
    channel.send(editor_automation::protocolHello());
    editor_automation::validateHandshake(channel.wait(std::chrono::seconds(5)));
    editor_automation::SessionController session(channel, channel.wait(std::chrono::seconds(5)));
    ValidationDiagnostics diagnostics;
    try {
      {
        EditorApplication application(session.resourceRoot(), session.initialPath(), diagnostics, nullptr, &session);
        channel.setWake(EditorGlfwBridge::postEmptyEvent);
        application.run();
      }
      std::cerr << "Automation Vulkan validation errors after teardown: " << diagnostics.errorCount() << '\n';
      if (diagnostics.errorCount() != 0) {
        return 3;
      }
    } catch (const std::exception& exception) {
      session.detach();
      std::cerr << "Automation editor failed: " << exception.what() << '\n';
      std::cerr << "Automation Vulkan validation errors after teardown: " << diagnostics.errorCount() << '\n';
      if (!channel.closed()) session.startupFailed(exception.what());
      channel.flush(std::chrono::seconds(1));
      return 2;
    }
    channel.flush(std::chrono::seconds(1));
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "Automation startup refused: " << exception.what() << '\n';
    return 2;
  }
}
