set(RENDERER_SOURCE "${SOURCE_ROOT}/src/core/render/renderer.cpp")
file(READ "${RENDERER_SOURCE}" RENDERER_CONTENT)
if(RENDERER_CONTENT MATCHES "pollEvents|waitEvents|shouldClose")
    message(FATAL_ERROR "Renderer source controls platform events or close state")
endif()

foreach(RUNTIME_SOURCE IN ITEMS
        "${SOURCE_ROOT}/src/core/application.cpp"
        "${SOURCE_ROOT}/src/core/engine.cpp"
        "${SOURCE_ROOT}/src/core/engine.hpp")
    file(READ "${RUNTIME_SOURCE}" RUNTIME_CONTENT)
    if(RUNTIME_CONTENT MATCHES "vulkan[/\\\\]|Vk[A-Z]|GLFW|glfw3")
        message(FATAL_ERROR "Backend dependency leaked into ${RUNTIME_SOURCE}")
    endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/core/platform/window.cpp" WINDOW_CONTENT)
if(NOT WINDOW_CONTENT MATCHES
   "void Window::waitEvents\\(\\)[^{]*\\{[^}]*beginEventBatch\\(\\)[^}]*glfwWaitEvents\\(\\)")
    message(FATAL_ERROR
        "Blocking window waits must begin an input event batch before dispatch")
endif()

file(READ "${SOURCE_ROOT}/src/core/engine.cpp" ENGINE_CONTENT)
if(NOT ENGINE_CONTENT MATCHES
   "window_\\.waitEvents\\(\\)[^;]*;[^;]*input_[^;]*window_\\.input\\(\\)")
    message(FATAL_ERROR
        "Engine must sample the waited input batch immediately after dispatch")
endif()
if(ENGINE_CONTENT MATCHES
   "static_cast<void>\\([^)]*renderFrame|[\r\n][ \t]*renderer_\\.renderFrame")
    message(FATAL_ERROR
        "Engine must consume each renderer frame outcome before continuing")
endif()

file(READ "${SOURCE_ROOT}/src/main.cpp" LAUNCHER_CONTENT)
if(LAUNCHER_CONTENT MATCHES "argv[ \\t\\r\\n]*\\[")
    message(FATAL_ERROR
        "Launcher must not derive its runtime layout from argv[0]")
endif()
if(NOT LAUNCHER_CONTENT MATCHES "executableResourceRoot\\(\\)")
    message(FATAL_ERROR
        "Launcher must derive resources from the native executable path")
endif()

string(FIND "${RENDERER_CONTENT}"
       "requireColorAttachmentSwapchainUsage" USAGE_VALIDATION_POSITION)
string(FIND "${RENDERER_CONTENT}"
       "chooseCompositeAlpha" COMPOSITE_SELECTION_POSITION)
string(FIND "${RENDERER_CONTENT}"
       "vkCreateSwapchainKHR" SWAPCHAIN_CREATION_POSITION)
if(USAGE_VALIDATION_POSITION LESS 0 OR
   COMPOSITE_SELECTION_POSITION LESS 0 OR
   SWAPCHAIN_CREATION_POSITION LESS 0 OR
   USAGE_VALIDATION_POSITION GREATER SWAPCHAIN_CREATION_POSITION OR
   COMPOSITE_SELECTION_POSITION GREATER SWAPCHAIN_CREATION_POSITION)
    message(FATAL_ERROR
        "Swapchain capabilities must be validated before vkCreateSwapchainKHR")
endif()

file(READ "${BUILD_ROOT}/compile_commands.json" COMPILE_COMMANDS)
string(JSON COMMAND_COUNT LENGTH "${COMPILE_COMMANDS}")
math(EXPR LAST_COMMAND "${COMMAND_COUNT} - 1")
set(REQUIRED_RUNTIME_FILES
        "src/core/application.cpp"
        "src/core/engine.cpp"
        "src/core/input/fps_input.cpp"
        "src/core/runtime_resources.cpp"
        "tests/boundary/minimal_consumer.cpp")
foreach(RUNTIME_FILE IN LISTS REQUIRED_RUNTIME_FILES)
    set(FOUND_RUNTIME_FILE FALSE)
    foreach(COMMAND_INDEX RANGE 0 ${LAST_COMMAND})
        string(JSON COMMAND_FILE GET "${COMPILE_COMMANDS}" ${COMMAND_INDEX} file)
        string(REPLACE "\\" "/" COMMAND_FILE "${COMMAND_FILE}")
        if(COMMAND_FILE MATCHES "${RUNTIME_FILE}$")
            set(FOUND_RUNTIME_FILE TRUE)
            string(JSON COMMAND_LINE GET
                   "${COMPILE_COMMANDS}" ${COMMAND_INDEX} command)
            if(COMMAND_LINE MATCHES "VulkanSDK|_deps[/\\\\]glfw-src")
                message(FATAL_ERROR
                    "Backend include directory leaked into ${RUNTIME_FILE}: "
                    "${COMMAND_LINE}")
            endif()
        endif()
    endforeach()
    if(NOT FOUND_RUNTIME_FILE)
        message(FATAL_ERROR "No compile command found for ${RUNTIME_FILE}")
    endif()
endforeach()
