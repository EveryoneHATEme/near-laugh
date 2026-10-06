#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "core/render/validation_diagnostics.hpp"
#include "development/narrative_fixture.hpp"

int main(int argc, char** argv) {
  try {
    std::filesystem::path resources = std::filesystem::absolute("resources"),
                          output, level;
    std::string_view mode = "--preflight";
    bool device = false;
    for (int i = 1; i < argc; ++i) {
      const std::string_view argument = argv[i];
      if (argument == "--preflight" || argument == "--checks" ||
          argument == "--play")
        mode = argument;
      else if (argument == "--device")
        device = true;
      else if ((argument == "--resources" || argument == "--output" ||
                argument == "--level") &&
               i + 1 < argc) {
        const auto value = std::filesystem::path(argv[++i]);
        if (argument == "--resources")
          resources = std::filesystem::absolute(value);
        if (argument == "--output") output = std::filesystem::absolute(value);
        if (argument == "--level") level = value;
      } else
        throw std::runtime_error(
            "Usage: narrative_fixture [--preflight|--checks|--play] "
            "[--resources DIR] [--output NEW_DIR] [--level FILE] [--device]");
    }
    if (output.empty())
      output = std::filesystem::absolute("build/narrative-checks") /
               std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count());
    std::cout << "Narrative report: " << output / "report.json" << '\n';
    ValidationDiagnostics diagnostics;
    if (mode == "--preflight")
      preflightNarrativeFixtures(resources, output);
    else if (mode == "--checks")
      runNarrativeFixtureChecks(resources, output, diagnostics);
    else
      playNarrativeFixture(resources, level, output, diagnostics, device);
    return diagnostics.errorCount() ? 2 : 0;
  } catch (const std::exception& error) {
    std::cerr << "Narrative fixture: " << error.what() << '\n';
    return 1;
  }
}
