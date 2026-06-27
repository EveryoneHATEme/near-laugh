#include <gtest/gtest.h>

#include <type_traits>

#include "src/core/application.hpp"

class ApplicationTestCase : public ::testing::Test {};

TEST_F(ApplicationTestCase, ApplicationOwnsRuntimeResources) {
  EXPECT_FALSE(std::is_copy_constructible_v<Application>);
  EXPECT_FALSE(std::is_copy_assignable_v<Application>);
}
