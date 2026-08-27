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

add_library(engine_lib STATIC
    src/core/application.cpp
    src/core/engine.cpp
    src/core/platform/input.cpp
    src/core/platform/platform.cpp
    src/core/platform/window.cpp
    src/core/render/graphics_pipeline.cpp
    src/core/render/renderer.cpp
    src/core/render/vulkan_context.cpp
    src/core/render/vulkan_utils.cpp
    src/core/resources/shader_provider.cpp
)
target_include_directories(engine_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(engine_lib PUBLIC Vulkan::Vulkan PRIVATE glfw)
near_laugh_enable_warnings(engine_lib)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(engine_lib PRIVATE NEAR_LAUGH_ENABLE_VULKAN_VALIDATION=1)
endif()

add_executable(fps src/main.cpp)
target_link_libraries(fps PRIVATE engine_lib)
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
