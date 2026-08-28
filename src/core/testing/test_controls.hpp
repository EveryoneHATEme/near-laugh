#ifndef CORE_TESTING_TEST_CONTROLS_HPP
#define CORE_TESTING_TEST_CONTROLS_HPP

#include <string>
#include <string_view>
#include <vector>

[[nodiscard]] bool forcedPlatformInitializationFailure() noexcept;
[[nodiscard]] bool forcedVulkanFailureAt(const char* stage) noexcept;
void setLifecycleLog(std::vector<std::string>* events) noexcept;
void recordLifecycleEvent(std::string_view event) noexcept;

#endif
