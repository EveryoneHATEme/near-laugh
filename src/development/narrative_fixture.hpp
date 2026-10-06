#ifndef DEVELOPMENT_NARRATIVE_FIXTURE_HPP
#define DEVELOPMENT_NARRATIVE_FIXTURE_HPP

#include <filesystem>

class ValidationDiagnostics;

// Explicit development entry points; neither is invoked by ordinary level
// names.
void preflightNarrativeFixtures(const std::filesystem::path& resources,
                                const std::filesystem::path& report_directory);
void runNarrativeFixtureChecks(const std::filesystem::path& resources,
                               const std::filesystem::path& report_directory,
                               ValidationDiagnostics& diagnostics);
void playNarrativeFixture(const std::filesystem::path& resources,
                          const std::filesystem::path& level,
                          const std::filesystem::path& report_directory,
                          ValidationDiagnostics& diagnostics, bool device);

#endif
