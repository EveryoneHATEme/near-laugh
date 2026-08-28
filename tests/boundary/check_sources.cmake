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
