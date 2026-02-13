cmake_minimum_required(VERSION 3.14)
project(engine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.0
)
FetchContent_MakeAvailable(SDL3)

# Main executable
add_library(engine_lib STATIC
    src/core/application.cpp
    src/core/render/renderer.cpp
    src/core/render/graphics_pipeline.cpp
)

set(ENGINE_LIB_LIBS
    SDL3::SDL3
)

target_link_libraries(engine_lib PUBLIC
    ${ENGINE_LIB_LIBS}
)

add_executable(
    engine
    src/main.cpp
)

target_link_libraries(engine PRIVATE
    engine_lib
)

if(WIN32)
    add_custom_command(TARGET engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:engine>"
        "$<TARGET_FILE_DIR:engine>"
        COMMAND_EXPAND_LISTS
    )
endif()

# Tests
enable_testing()
add_subdirectory(tests)
