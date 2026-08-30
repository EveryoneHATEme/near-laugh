cmake_minimum_required(VERSION 3.25)
project(near_laugh LANGUAGES C CXX)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
        "near-laugh requires the Clang toolchain for both C and C++; "
        "configured compiler IDs are C='${CMAKE_C_COMPILER_ID}' "
        "(${CMAKE_C_COMPILER}) and C++='${CMAKE_CXX_COMPILER_ID}' "
        "(${CMAKE_CXX_COMPILER}).")
endif()

if(WIN32 AND (NOT CMAKE_C_SIMULATE_ID STREQUAL "MSVC" OR
              NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
    message(FATAL_ERROR
        "near-laugh requires Clang to target the MSVC-compatible Windows ABI "
        "for both C and C++; configured simulation IDs are "
        "C='${CMAKE_C_SIMULATE_ID}' and C++='${CMAKE_CXX_SIMULATE_ID}'.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(CTest)

find_package(Vulkan 1.3 REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DISTRIBUTION OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DISTRIBUTION OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.6.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(JoltPhysics)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(near_laugh_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(near_laugh_world STATIC
    src/core/world/prototype_level.cpp
)
target_include_directories(near_laugh_world PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
near_laugh_enable_warnings(near_laugh_world)

add_library(near_laugh_physics STATIC
    src/core/physics/physics_world.cpp
)
target_include_directories(near_laugh_physics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
get_target_property(jolt_include_directories Jolt INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(near_laugh_physics SYSTEM PRIVATE
    ${jolt_include_directories}
)
target_link_libraries(near_laugh_physics PRIVATE near_laugh_world Jolt)
near_laugh_enable_warnings(near_laugh_physics)

add_library(near_laugh_platform STATIC
    src/core/platform/input.cpp
    src/core/platform/platform.cpp
    src/core/platform/window.cpp
    src/core/testing/test_controls.cpp
)
target_include_directories(near_laugh_platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(near_laugh_platform PRIVATE glfw)
near_laugh_enable_warnings(near_laugh_platform)

add_library(near_laugh_render STATIC
    src/core/platform/glfw_vulkan_bridge.cpp
    src/core/render/depth_attachment.cpp
    src/core/render/graphics_pipeline.cpp
    src/core/render/prototype_scene.cpp
    src/core/render/renderer.cpp
    src/core/render/validation_diagnostics.cpp
    src/core/render/vulkan_context.cpp
    src/core/render/vulkan_utils.cpp
    src/core/resources/shader_provider.cpp
)
target_include_directories(near_laugh_render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(near_laugh_render
    PRIVATE near_laugh_platform near_laugh_world Vulkan::Vulkan glfw
)
near_laugh_enable_warnings(near_laugh_render)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(near_laugh_render
        PRIVATE NEAR_LAUGH_ENABLE_VULKAN_VALIDATION=1
    )
endif()

add_library(near_laugh_runtime STATIC
    src/core/application.cpp
    src/core/engine.cpp
    src/core/gameplay/prototype_rifle.cpp
    src/core/gameplay/shooting_range.cpp
    src/core/gameplay/shooting_targets.cpp
    src/core/input/fps_input.cpp
    src/core/player/player_controller.cpp
    src/core/runtime_resources.cpp
    src/core/simulation/fixed_step.cpp
)
target_include_directories(near_laugh_runtime
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${glm_SOURCE_DIR}
)
target_link_libraries(near_laugh_runtime
    PRIVATE near_laugh_platform near_laugh_world near_laugh_physics
            near_laugh_render
)
target_compile_definitions(near_laugh_runtime PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)
near_laugh_enable_warnings(near_laugh_runtime)

get_target_property(near_laugh_runtime_interface_includes
                    near_laugh_runtime INTERFACE_INCLUDE_DIRECTORIES)
if(NOT near_laugh_runtime_interface_includes STREQUAL
       "${CMAKE_CURRENT_SOURCE_DIR}/include")
    message(FATAL_ERROR
        "near_laugh_runtime exposes unexpected include directories: "
        "${near_laugh_runtime_interface_includes}")
endif()
get_target_property(near_laugh_runtime_interface_links
                    near_laugh_runtime INTERFACE_LINK_LIBRARIES)
if(near_laugh_runtime_interface_links MATCHES "Vulkan::Vulkan|glfw|glm|Jolt")
    message(FATAL_ERROR
        "near_laugh_runtime exposes a backend usage requirement: "
        "${near_laugh_runtime_interface_links}")
endif()
get_target_property(near_laugh_runtime_interface_definitions
                    near_laugh_runtime INTERFACE_COMPILE_DEFINITIONS)
if(near_laugh_runtime_interface_definitions MATCHES "GLM_")
    message(FATAL_ERROR
        "near_laugh_runtime exposes GLM compile definitions: "
        "${near_laugh_runtime_interface_definitions}")
endif()

add_executable(fps
    src/main.cpp
    src/launcher/executable_path.cpp
)
target_include_directories(fps PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fps PRIVATE near_laugh_runtime)
near_laugh_enable_warnings(fps)
set_target_properties(fps PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_custom_command(TARGET fps POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders"
            "$<TARGET_FILE_DIR:fps>/resources/shaders"
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
