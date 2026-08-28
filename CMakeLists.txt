cmake_minimum_required(VERSION 3.25)
project(near_laugh LANGUAGES CXX)

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
    src/core/render/graphics_pipeline.cpp
    src/core/render/renderer.cpp
    src/core/render/validation_diagnostics.cpp
    src/core/render/vulkan_context.cpp
    src/core/render/vulkan_utils.cpp
    src/core/resources/shader_provider.cpp
)
target_include_directories(near_laugh_render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(near_laugh_render
    PRIVATE near_laugh_platform Vulkan::Vulkan glfw
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
    src/core/input/fps_input.cpp
    src/core/runtime_resources.cpp
)
target_include_directories(near_laugh_runtime
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(near_laugh_runtime
    PRIVATE near_laugh_platform near_laugh_render
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
if(near_laugh_runtime_interface_links MATCHES "Vulkan::Vulkan|glfw")
    message(FATAL_ERROR
        "near_laugh_runtime exposes a backend usage requirement: "
        "${near_laugh_runtime_interface_links}")
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
