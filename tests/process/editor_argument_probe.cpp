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
