#include <cerrno>
#include <cstring>
#include <thread>

#include "editor/editor_playtest.hpp"
#include "launcher/executable_path.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace {
std::string pathText(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}
#if defined(_WIN32)
// CreateProcess receives one command line; quote for the native CRT argv
// parser.
std::wstring quoteArgument(std::wstring_view value) {
  std::wstring quoted{L"\""};
  std::size_t slashes = 0;
  for (wchar_t c : value) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    quoted.append(slashes * (c == L'"' ? 2 : 1), L'\\');
    slashes = 0;
    if (c == L'"') quoted += L'\\';
    quoted += c;
  }
  quoted.append(slashes * 2, L'\\');
  quoted += L'"';
  return quoted;
}
#endif
}  // namespace

class EditorGameProcess::Impl {
 public:
  ~Impl() {
#if defined(_WIN32)
    if (process) CloseHandle(process);
#else
    if (process > 0) {
      const auto child = process;
      // No editor resources survive here; only the wait obligation remains.
      std::thread([child] {
        int status;
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
        }
      }).detach();
    }
#endif
  }
  bool active() const noexcept {
#if defined(_WIN32)
    return process != nullptr;
#else
    return process > 0;
#endif
  }
  bool start(const std::filesystem::path& executable,
             const EditorLaunchRequest& request) {
    if (active()) {
      message = "A game process is already active.";
      return false;
    }
    context = pathText(executable) + " | level " +
              pathText(request.level_path) + " | entry " + request.entry_id;
    const auto fail = [&](std::string reason) {
      message = "Launch failed: " + context + ": " + reason;
      return false;
    };
    if (!executable.is_absolute() || !request.level_path.is_absolute() ||
        !levelEntryIdIsValid(request.entry_id))
      return fail("absolute paths and a valid entry ID are required");
    std::error_code error;
    if (!std::filesystem::is_regular_file(executable, error))
      return fail("game executable is missing");
#if defined(_WIN32)
    const std::wstring entry(request.entry_id.begin(), request.entry_id.end());
    std::wstring command = quoteArgument(executable.native()) + L" --level " +
                           quoteArgument(request.level_path.native()) +
                           L" --entry " + quoteArgument(entry);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION created{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                        &created))
      return fail("CreateProcessW error " + std::to_string(GetLastError()));
    CloseHandle(created.hThread);
    process = created.hProcess;
#else
    std::string native_executable = executable.native();
    std::string level = request.level_path.native();
    std::string entry = request.entry_id;
    char* arguments[] = {native_executable.data(),
                         const_cast<char*>("--level"),
                         level.data(),
                         const_cast<char*>("--entry"),
                         entry.data(),
                         nullptr};
    pid_t child{};
    const int result = posix_spawn(&child, native_executable.c_str(), nullptr,
                                   nullptr, arguments, environ);
    if (result != 0) return fail(std::strerror(result));
    process = child;
#endif
    message = "Process created; game startup is not yet confirmed: " + context;
    return true;
  }
  void poll() {
    if (!active()) return;
#if defined(_WIN32)
    const DWORD result = WaitForSingleObject(process, 0);
    if (result == WAIT_TIMEOUT) return;
    if (result == WAIT_FAILED) {
      message = "Could not observe game process: " + context + ": error " +
                std::to_string(GetLastError());
      return;
    }
    DWORD code{};
    const bool observed = GetExitCodeProcess(process, &code) != FALSE;
    CloseHandle(process);
    process = nullptr;
    message = observed ? "Game exited with code " + std::to_string(code) +
                             ": " + context
                       : "Game exited; exit code unavailable: " + context;
#else
    int status{};
    const auto result = waitpid(process, &status, WNOHANG);
    if (result == 0 || (result < 0 && errno == EINTR)) return;
    process = 0;
    if (result < 0)
      message = "Could not observe game exit: " + context + ": " +
                std::strerror(errno);
    else if (WIFEXITED(status))
      message = "Game exited with code " + std::to_string(WEXITSTATUS(status)) +
                ": " + context;
    else
      message = "Game terminated by signal " +
                std::to_string(WTERMSIG(status)) + ": " + context;
#endif
  }
  std::string message{};
  std::string context{};
#if defined(_WIN32)
  HANDLE process{};
#else
  pid_t process{};
#endif
};

EditorGameProcess::EditorGameProcess() : impl_(std::make_unique<Impl>()) {}
EditorGameProcess::~EditorGameProcess() = default;
bool EditorGameProcess::start(const std::filesystem::path& executable,
                              const EditorLaunchRequest& request) {
  return impl_->start(executable, request);
}
void EditorGameProcess::poll() { impl_->poll(); }
bool EditorGameProcess::active() const noexcept { return impl_->active(); }
const std::string& EditorGameProcess::status() const noexcept {
  return impl_->message;
}

std::filesystem::path editorGameExecutable() {
  return launcher::currentExecutablePath().parent_path() /
#if defined(_WIN32)
         "near_laugh.exe";
#else
         "near_laugh";
#endif
}
