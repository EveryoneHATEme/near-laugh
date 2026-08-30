set(RENDERER_SOURCE "${SOURCE_ROOT}/src/core/render/renderer.cpp")
file(READ "${RENDERER_SOURCE}" RENDERER_CONTENT)
if(RENDERER_CONTENT MATCHES "pollEvents|waitEvents|shouldClose")
    message(FATAL_ERROR "Renderer source controls platform events or close state")
endif()
file(GLOB_RECURSE RENDER_SOURCES
     "${SOURCE_ROOT}/src/core/render/*.cpp"
     "${SOURCE_ROOT}/src/core/render/*.hpp")
foreach(RENDER_SOURCE IN LISTS RENDER_SOURCES)
    file(READ "${RENDER_SOURCE}" RENDER_SOURCE_CONTENT)
    if(RENDER_SOURCE_CONTENT MATCHES
       "FpsActionSnapshot|PlayerController|FixedStep|steady_clock|<chrono>|free_fly_camera|Jolt[/\\\\]|JPH::")
        message(FATAL_ERROR
            "Renderer source consumes FPS actions, camera policy, or timing: "
            "${RENDER_SOURCE}")
    endif()
endforeach()

file(GLOB_RECURSE PROJECT_BACKEND_SOURCES
     "${SOURCE_ROOT}/src/core/*.cpp"
     "${SOURCE_ROOT}/src/core/*.hpp")
foreach(PROJECT_SOURCE IN LISTS PROJECT_BACKEND_SOURCES)
    if(PROJECT_SOURCE MATCHES "[/\\\\]physics[/\\\\]")
        continue()
    endif()
    file(READ "${PROJECT_SOURCE}" PROJECT_SOURCE_CONTENT)
    if(PROJECT_SOURCE_CONTENT MATCHES "Jolt[/\\\\]|JPH::")
        message(FATAL_ERROR
            "Jolt dependency escaped the physics module: ${PROJECT_SOURCE}")
    endif()
endforeach()

file(GLOB_RECURSE PHYSICS_SOURCES
     "${SOURCE_ROOT}/src/core/physics/*.cpp"
     "${SOURCE_ROOT}/src/core/physics/*.hpp")
foreach(PHYSICS_SOURCE IN LISTS PHYSICS_SOURCES)
    file(READ "${PHYSICS_SOURCE}" PHYSICS_SOURCE_CONTENT)
    if(PHYSICS_SOURCE_CONTENT MATCHES
       "FpsActionSnapshot|fps_input|GLFW|glfw3|vulkan[/\\\\]|Vk[A-Z]|core/render|core/platform")
        message(FATAL_ERROR
            "Physics source depends on input, platform, or rendering: "
            "${PHYSICS_SOURCE}")
    endif()
endforeach()
file(READ "${SOURCE_ROOT}/src/core/physics/physics_world.cpp" PHYSICS_WORLD_CONTENT)
if(PHYSICS_WORLD_CONTENT MATCHES "JobSystemThreadPool")
    message(FATAL_ERROR "Physics world introduced a worker pool")
endif()
if(NOT PHYSICS_WORLD_CONTENT MATCHES "JobSystemSingleThreaded")
    message(FATAL_ERROR "Physics world is missing the single-thread job system")
endif()
if(RENDERER_CONTENT MATCHES
   "vkCreateRenderPass|vkCmdPipelineBarrier\\(|vkQueueSubmit\\(")
    message(FATAL_ERROR
        "Renderer introduced a legacy render pass or synchronization API")
endif()
foreach(REQUIRED_DEPTH_TOKEN IN ITEMS
        "vkCmdPipelineBarrier2"
        "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL"
        "pDepthAttachment")
    if(NOT RENDERER_CONTENT MATCHES "${REQUIRED_DEPTH_TOKEN}")
        message(FATAL_ERROR
            "Renderer is missing required depth path: ${REQUIRED_DEPTH_TOKEN}")
    endif()
endforeach()

file(GLOB_RECURSE RUNTIME_HEADERS
     "${SOURCE_ROOT}/include/*.hpp"
     "${SOURCE_ROOT}/src/core/gameplay/*.hpp"
     "${SOURCE_ROOT}/src/core/player/*.hpp"
     "${SOURCE_ROOT}/src/core/physics/*.hpp"
     "${SOURCE_ROOT}/src/core/simulation/*.hpp"
     "${SOURCE_ROOT}/src/core/world/*.hpp")
list(APPEND RUNTIME_HEADERS "${SOURCE_ROOT}/src/core/frame.hpp")
foreach(RUNTIME_HEADER IN LISTS RUNTIME_HEADERS)
    file(READ "${RUNTIME_HEADER}" RUNTIME_HEADER_CONTENT)
    if(RUNTIME_HEADER_CONTENT MATCHES
       "vulkan[/\\\\]|Vk[A-Z]|GLFW|glfw3|glm[/\\\\]|Jolt[/\\\\]|JPH::")
        message(FATAL_ERROR
            "Backend or GLM type leaked into runtime header: ${RUNTIME_HEADER}")
    endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/core/render/graphics_pipeline.cpp"
     PIPELINE_CONTENT)
file(READ "${SOURCE_ROOT}/src/core/render/graphics_pipeline.hpp"
     PIPELINE_HEADER_CONTENT)
foreach(REQUIRED_PIPELINE_TOKEN IN ITEMS
        "depthTestEnable"
        "depthWriteEnable"
        "depthAttachmentFormat"
        "vkCmdPushConstants")
    if(NOT PIPELINE_CONTENT MATCHES "${REQUIRED_PIPELINE_TOKEN}")
        message(FATAL_ERROR
            "Graphics pipeline is missing: ${REQUIRED_PIPELINE_TOKEN}")
    endif()
endforeach()
foreach(REQUIRED_PRESENTATION_TOKEN IN ITEMS
        "request.scene_presentation"
        "presentation_masks"
        "solid_mask")
    string(FIND "${RENDERER_CONTENT}${PIPELINE_CONTENT}${PIPELINE_HEADER_CONTENT}"
           "${REQUIRED_PRESENTATION_TOKEN}" PRESENTATION_TOKEN_POSITION)
    if(PRESENTATION_TOKEN_POSITION LESS 0)
        message(FATAL_ERROR
            "Renderer is missing solid-mask presentation flow: "
            "${REQUIRED_PRESENTATION_TOKEN}")
    endif()
endforeach()
if(RENDERER_CONTENT MATCHES
   "vkCmdDrawIndexed|vkCmdBindDescriptorSets|vkUpdateDescriptorSets")
    message(FATAL_ERROR
        "Prototype solid presentation introduced extra geometry or descriptors")
endif()
string(REGEX MATCHALL "vkCmdDraw\\(" SCENE_DRAW_CALLS "${PIPELINE_CONTENT}")
list(LENGTH SCENE_DRAW_CALLS SCENE_DRAW_CALL_COUNT)
if(NOT SCENE_DRAW_CALL_COUNT EQUAL 1)
    message(FATAL_ERROR "Prototype scene must retain exactly one draw command")
endif()

foreach(RUNTIME_SOURCE IN ITEMS
        "${SOURCE_ROOT}/src/core/application.cpp"
        "${SOURCE_ROOT}/src/core/engine.cpp"
        "${SOURCE_ROOT}/src/core/engine.hpp")
    file(READ "${RUNTIME_SOURCE}" RUNTIME_CONTENT)
    if(RUNTIME_CONTENT MATCHES "vulkan[/\\\\]|Vk[A-Z]|GLFW|glfw3|glm[/\\\\]")
        message(FATAL_ERROR "Backend dependency leaked into ${RUNTIME_SOURCE}")
    endif()
endforeach()

file(READ "${SOURCE_ROOT}/CMakeLists.txt" ROOT_CMAKE_CONTENT)
if(NOT ROOT_CMAKE_CONTENT MATCHES
   "target_link_libraries\\(near_laugh_physics PRIVATE near_laugh_world Jolt\\)")
    message(FATAL_ERROR
        "Jolt must be linked privately and only by near_laugh_physics")
endif()
string(REGEX MATCHALL "target_link_libraries\\([^)]*Jolt[^)]*\\)"
       JOLT_LINK_BLOCKS "${ROOT_CMAKE_CONTENT}")
list(LENGTH JOLT_LINK_BLOCKS JOLT_LINK_BLOCK_COUNT)
if(NOT JOLT_LINK_BLOCK_COUNT EQUAL 1)
    message(FATAL_ERROR "Jolt is linked by a target other than physics")
endif()

file(READ "${SOURCE_ROOT}/src/core/platform/window.cpp" WINDOW_CONTENT)
if(NOT WINDOW_CONTENT MATCHES
   "void Window::waitEvents\\(\\)[^{]*\\{[^}]*beginEventBatch\\(\\)[^}]*glfwWaitEvents\\(\\)")
    message(FATAL_ERROR
        "Blocking window waits must begin an input event batch before dispatch")
endif()

file(READ "${SOURCE_ROOT}/src/core/engine.cpp" ENGINE_CONTENT)
file(READ "${SOURCE_ROOT}/src/core/engine.hpp" ENGINE_HEADER_CONTENT)
set(PREVIOUS_LIFETIME_POSITION -1)
foreach(LIFETIME_MEMBER IN ITEMS
        "Platform platform_"
        "Window window_"
        "PrototypeLevel level_"
        "PhysicsWorld physics_"
        "PlayerController player_"
        "PrototypeRifle rifle_"
        "ShootingTargets targets_"
        "Renderer renderer_")
    string(FIND "${ENGINE_HEADER_CONTENT}" "${LIFETIME_MEMBER}"
           LIFETIME_POSITION)
    if(LIFETIME_POSITION LESS 0 OR
       LIFETIME_POSITION LESS PREVIOUS_LIFETIME_POSITION)
        message(FATAL_ERROR
            "Engine subsystem lifetime order is invalid at ${LIFETIME_MEMBER}")
    endif()
    set(PREVIOUS_LIFETIME_POSITION ${LIFETIME_POSITION})
endforeach()
if(NOT ENGINE_CONTENT MATCHES
   "window_\\.waitEvents\\(\\)[^;]*;[^;]*input_[^;]*window_\\.input\\(\\)")
    message(FATAL_ERROR
        "Engine must sample the waited input batch immediately after dispatch")
endif()
if(NOT ENGINE_CONTENT MATCHES
   "window_\\.waitEvents\\(\\)[^;]*;[^}]*input_[^;]*window_\\.input\\(\\)[^}]*samplePlayerInput\\(input_\\)[^}]*fixed_step_\\.reset\\(\\)")
    message(FATAL_ERROR
        "Engine must apply waited input before resetting fixed-step timing")
endif()
if(ENGINE_CONTENT MATCHES
   "static_cast<void>\\([^)]*renderFrame|[\r\n][ \t]*renderer_\\.renderFrame")
    message(FATAL_ERROR
        "Engine must consume each renderer frame outcome before continuing")
endif()
foreach(REQUIRED_RUNTIME_CAMERA_TOKEN IN ITEMS
        "setCursorCaptured(true)"
        "playerCursorTransition"
        "FixedStepAccumulator::Clock::now"
        "player_.sampleInput"
        "coordinateShootingRangeFixedStep"
        "frame.camera = player_.cameraFrame")
    string(FIND "${ENGINE_CONTENT}"
           "${REQUIRED_RUNTIME_CAMERA_TOKEN}" CAMERA_TOKEN_POSITION)
    if(CAMERA_TOKEN_POSITION LESS 0)
        message(FATAL_ERROR
            "Engine is missing runtime camera coordination: "
            "${REQUIRED_RUNTIME_CAMERA_TOKEN}")
    endif()
endforeach()
if(NOT ENGINE_CONTENT MATCHES
   "window_\\.waitEvents\\(\\)[^;]*;[^}]*fixed_step_\\.reset\\(\\)")
    message(FATAL_ERROR
        "Engine must reset frame timing after a blocking window wait")
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
        "src/core/player/player_controller.cpp"
        "src/core/runtime_resources.cpp"
        "src/core/simulation/fixed_step.cpp"
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

set(REQUIRED_NON_PHYSICS_FILES
        "src/core/application.cpp"
        "src/core/engine.cpp"
        "src/core/input/fps_input.cpp"
        "src/core/render/renderer.cpp"
        "src/core/world/prototype_level.cpp"
        "tests/boundary/minimal_consumer.cpp")
foreach(PROJECT_FILE IN LISTS REQUIRED_NON_PHYSICS_FILES)
    foreach(COMMAND_INDEX RANGE 0 ${LAST_COMMAND})
        string(JSON COMMAND_FILE GET "${COMPILE_COMMANDS}" ${COMMAND_INDEX} file)
        string(REPLACE "\\" "/" COMMAND_FILE "${COMMAND_FILE}")
        if(COMMAND_FILE MATCHES "${PROJECT_FILE}$")
            string(JSON COMMAND_LINE GET
                   "${COMPILE_COMMANDS}" ${COMMAND_INDEX} command)
            if(COMMAND_LINE MATCHES "joltphysics-src|[/\\\\]Jolt[/\\\\]")
                message(FATAL_ERROR
                    "Jolt include requirement leaked into ${PROJECT_FILE}: "
                    "${COMMAND_LINE}")
            endif()
        endif()
    endforeach()
endforeach()
