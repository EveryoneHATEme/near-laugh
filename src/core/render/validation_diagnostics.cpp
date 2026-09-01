#include "core/render/validation_diagnostics.hpp"

#include <iostream>

std::string_view validationSeverityName(ValidationSeverity severity) noexcept {
  switch (severity) {
    case ValidationSeverity::Verbose:
      return "verbose";
    case ValidationSeverity::Info:
      return "info";
    case ValidationSeverity::Warning:
      return "warning";
    case ValidationSeverity::Error:
      return "error";
  }
  return "unknown";
}

std::string_view validationCategoryName(ValidationCategory category) noexcept {
  switch (category) {
    case ValidationCategory::General:
      return "general";
    case ValidationCategory::Validation:
      return "validation";
    case ValidationCategory::Performance:
      return "performance";
    case ValidationCategory::Multiple:
      return "multiple";
  }
  return "unknown";
}

void ValidationDiagnostics::record(ValidationSeverity severity,
                                   ValidationCategory category,
                                   std::string_view message) noexcept {
  if (severity == ValidationSeverity::Error) {
    error_count_.fetch_add(1, std::memory_order_relaxed);
  }
  try {
    {
      std::lock_guard lock(messages_mutex_);
      messages_.push_back({severity, category, std::string(message)});
    }
    std::cerr << "Vulkan validation [" << validationSeverityName(severity)
              << ", " << validationCategoryName(category) << "]: " << message
              << '\n';
  } catch (...) {
    // Vulkan callbacks must never propagate exceptions.
  }
}

std::size_t ValidationDiagnostics::errorCount() const noexcept {
  return error_count_.load(std::memory_order_relaxed);
}

std::vector<ValidationMessage> ValidationDiagnostics::messages() const {
  std::lock_guard lock(messages_mutex_);
  return messages_;
}
