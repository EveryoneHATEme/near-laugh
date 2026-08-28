#ifndef CORE_RENDER_VALIDATION_DIAGNOSTICS_HPP
#define CORE_RENDER_VALIDATION_DIAGNOSTICS_HPP

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

enum class ValidationSeverity { Verbose, Info, Warning, Error };
enum class ValidationCategory { General, Validation, Performance, Multiple };

struct ValidationMessage {
  ValidationSeverity severity{ValidationSeverity::Info};
  ValidationCategory category{ValidationCategory::General};
  std::string text{};
};

[[nodiscard]] std::string_view validationSeverityName(
    ValidationSeverity severity) noexcept;
[[nodiscard]] std::string_view validationCategoryName(
    ValidationCategory category) noexcept;

class ValidationDiagnostics {
 public:
  void record(ValidationSeverity severity, ValidationCategory category,
              std::string_view message) noexcept;

  [[nodiscard]] std::size_t errorCount() const noexcept;
  [[nodiscard]] std::vector<ValidationMessage> messages() const;

 private:
  std::atomic_size_t error_count_{};
  mutable std::mutex messages_mutex_{};
  std::vector<ValidationMessage> messages_{};
};

#endif
