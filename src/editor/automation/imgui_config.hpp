#pragma once

// Every translation unit in the instrumented executable uses this config.
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_ENABLE_TEST_ENGINE
#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1
#define IMGUI_TEST_ENGINE_ENABLE_CAPTURE 0
#define IMGUI_TEST_ENGINE_ENABLE_IMPLOT 0
#define IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION 0
#include <imgui_te_imconfig.h>
