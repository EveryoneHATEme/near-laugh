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

find_program(ENGINE_GLSLANG_VALIDATOR glslangValidator)
find_program(ENGINE_GLSLC glslc)

set(ENGINE_RESOURCE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/resources")
set(ENGINE_RESOURCE_BUILD_DIR "${CMAKE_BINARY_DIR}/resources")
set(ENGINE_SHADER_SOURCE_DIR "${ENGINE_RESOURCE_SOURCE_DIR}/shaders")
set(ENGINE_SHADER_BUILD_DIR "${ENGINE_RESOURCE_BUILD_DIR}/shaders")
set(ENGINE_SHADER_OUTPUTS)

function(add_engine_shader GLSL_SOURCE GLSLANG_STAGE GLSLC_STAGE SPV_FILE_NAME)
    set(SPV_OUTPUT "${ENGINE_SHADER_BUILD_DIR}/${SPV_FILE_NAME}")

    if(ENGINE_GLSLANG_VALIDATOR)
        set(SHADER_COMPILE_COMMAND
            "${ENGINE_GLSLANG_VALIDATOR}" -V -S "${GLSLANG_STAGE}"
            "${GLSL_SOURCE}" -o "${SPV_OUTPUT}"
        )
    elseif(ENGINE_GLSLC)
        set(SHADER_COMPILE_COMMAND
            "${ENGINE_GLSLC}" "-fshader-stage=${GLSLC_STAGE}"
            "${GLSL_SOURCE}" -o "${SPV_OUTPUT}"
        )
    else()
        return()
    endif()

    add_custom_command(
        OUTPUT "${SPV_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ENGINE_SHADER_BUILD_DIR}"
        COMMAND ${SHADER_COMPILE_COMMAND}
        DEPENDS "${GLSL_SOURCE}"
        COMMENT "Compiling GLSL shader ${GLSL_SOURCE}"
        VERBATIM
    )
    list(APPEND ENGINE_SHADER_OUTPUTS "${SPV_OUTPUT}")
    set(ENGINE_SHADER_OUTPUTS "${ENGINE_SHADER_OUTPUTS}" PARENT_SCOPE)
endfunction()

add_engine_shader(
    "${ENGINE_SHADER_SOURCE_DIR}/triangle_vertex.glsl"
    vert
    vertex
    triangle_vertex.spv
)
add_engine_shader(
    "${ENGINE_SHADER_SOURCE_DIR}/triangle_fragment.glsl"
    frag
    fragment
    triangle_fragment.spv
)

if(ENGINE_SHADER_OUTPUTS)
    add_custom_target(engine_shaders DEPENDS ${ENGINE_SHADER_OUTPUTS})
else()
    message(WARNING
        "No GLSL-to-SPIR-V compiler found. Install the Vulkan SDK "
        "or put glslangValidator/glslc on PATH to auto-compile shaders."
    )
endif()

# Main executable
add_library(engine_lib STATIC
    src/core/application.cpp
    src/core/game/camera.cpp
    src/core/game/game_world.cpp
    src/core/game/level.cpp
    src/core/game/player_controller.cpp
    src/core/input/input_state.cpp
    src/core/render/renderer.cpp
    src/core/render/graphics_pipeline.cpp
)

set(ENGINE_LIB_LIBS
    SDL3::SDL3
)

target_link_libraries(engine_lib PUBLIC
    ${ENGINE_LIB_LIBS}
)

target_include_directories(engine_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(engine_lib PUBLIC
    ENGINE_RESOURCE_DIR=\"${ENGINE_RESOURCE_BUILD_DIR}\"
)

add_executable(
    engine
    src/main.cpp
)

target_link_libraries(engine PRIVATE
    engine_lib
)

if(TARGET engine_shaders)
    add_dependencies(engine engine_shaders)
endif()

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
