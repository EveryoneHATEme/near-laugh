#include <gtest/gtest.h>

#include "src/core/application.hpp"

class ApplicationTestCase : public ::testing::Test {};

TEST_F(ApplicationTestCase, TestConstructor) { Application a; }
