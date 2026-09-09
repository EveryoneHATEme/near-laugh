#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
  if (argc != 5 || std::filesystem::path(argv[1]) != "--level" ||
      std::filesystem::path(argv[3]) != "--entry")
    return 90;
  const std::filesystem::path output_path(argv[2]);
  const auto entry = std::filesystem::path(argv[4]).u8string();
  if (entry == u8"probe-launch") {
    // Observe the actual saved scene without replacing it. Append once per
    // native child so tests still detect duplicate launches after process exit.
    std::ofstream launches(
        std::filesystem::path(output_path).concat(".launches"), std::ios::app);
    launches << "created\n";
    std::ifstream saved(output_path, std::ios::binary);
    std::ofstream observed(
        std::filesystem::path(output_path).concat(".observed"),
        std::ios::binary);
    observed << saved.rdbuf();
    std::ofstream arguments(
        std::filesystem::path(output_path).concat(".arguments"),
        std::ios::binary);
    const auto level = output_path.u8string();
    const auto cwd = std::filesystem::current_path().u8string();
    arguments << std::string(level.begin(), level.end()) << '\n'
              << std::string(entry.begin(), entry.end()) << '\n'
              << std::string(cwd.begin(), cwd.end()) << '\n';
    return saved && observed && launches && arguments ? 0 : 92;
  }
  {
    std::ofstream output(output_path, std::ios::binary);
    const auto path = output_path.u8string();
    output << std::string(path.begin(), path.end()) << '\n'
           << std::string(entry.begin(), entry.end()) << '\n';
    if (!output) return 91;
  }
  if (entry == u8"wait")
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
  std::ofstream done(std::filesystem::path(output_path).concat(".done"));
  done << "completed";
  return entry == u8"exit-seven" ? 7 : 0;
}
