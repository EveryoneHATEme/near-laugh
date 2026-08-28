#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "core/render/validation_diagnostics.hpp"

TEST(ValidationDiagnostics, DecodesSeverityAndCategoryNames) {
  EXPECT_EQ(validationSeverityName(ValidationSeverity::Verbose), "verbose");
  EXPECT_EQ(validationSeverityName(ValidationSeverity::Info), "info");
  EXPECT_EQ(validationSeverityName(ValidationSeverity::Warning), "warning");
  EXPECT_EQ(validationSeverityName(ValidationSeverity::Error), "error");
  EXPECT_EQ(validationCategoryName(ValidationCategory::General), "general");
  EXPECT_EQ(validationCategoryName(ValidationCategory::Validation),
            "validation");
  EXPECT_EQ(validationCategoryName(ValidationCategory::Performance),
            "performance");
  EXPECT_EQ(validationCategoryName(ValidationCategory::Multiple), "multiple");
}

TEST(ValidationDiagnostics, CountsErrorsSafelyAcrossThreads) {
  ValidationDiagnostics diagnostics;
  std::vector<std::thread> workers;
  for (int worker = 0; worker < 4; ++worker) {
    workers.emplace_back([&diagnostics] {
      for (int message = 0; message < 25; ++message) {
        diagnostics.record(ValidationSeverity::Error,
                           ValidationCategory::Validation, "test error");
      }
    });
  }
  for (std::thread& worker : workers) {
    worker.join();
  }
  EXPECT_EQ(diagnostics.errorCount(), 100U);
  EXPECT_EQ(diagnostics.messages().size(), 100U);
}
