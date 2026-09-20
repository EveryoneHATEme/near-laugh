# Kept behind NEAR_LAUGH_UI_AUTOMATION: ordinary configurations never fetch it.
FetchContent_Declare(imgui_test_engine
    GIT_REPOSITORY https://github.com/ocornut/imgui_test_engine.git
    GIT_TAG 2628e39cc0ea3a0a612d5d039543c9d4e873c720
    SOURCE_SUBDIR near_laugh_dependency_only
)
FetchContent_MakeAvailable(imgui_test_engine)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB_RECURSE editor_fingerprint_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
list(APPEND editor_fingerprint_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/editor_ui_protocol.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_editor_ui_contract.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/editor_ui_automation.cmake")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${editor_fingerprint_sources})
set(editor_build_identity "${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_VERSION};${CMAKE_BUILD_TYPE};${CMAKE_CXX_FLAGS};v1.92.9b-docking;2628e39cc0ea3a0a612d5d039543c9d4e873c720")
foreach(source IN LISTS editor_fingerprint_sources)
    file(SHA256 "${source}" source_hash)
    string(APPEND editor_build_identity ";${source_hash}")
endforeach()
string(SHA256 editor_build_fingerprint "${editor_build_identity}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/editor-automation")
execute_process(COMMAND ${Python3_EXECUTABLE} -B
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_editor_ui_contract.py"
    "${CMAKE_BINARY_DIR}/generated/editor-automation/editor_ui_contract.inc"
    "${editor_build_fingerprint}"
    COMMAND_ERROR_IS_FATAL ANY)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/bin/editor-ui-build.json"
    CONTENT "{\"build_fingerprint\":\"${editor_build_fingerprint}\"}\n")

get_target_property(editor_imgui_sources near_laugh_imgui SOURCES)
set(editor_test_engine_dir "${imgui_test_engine_SOURCE_DIR}/imgui_test_engine")
add_library(near_laugh_imgui_automation STATIC ${editor_imgui_sources}
    ${editor_test_engine_dir}/imgui_capture_tool.cpp
    ${editor_test_engine_dir}/imgui_te_context.cpp
    ${editor_test_engine_dir}/imgui_te_coroutine.cpp
    ${editor_test_engine_dir}/imgui_te_engine.cpp
    ${editor_test_engine_dir}/imgui_te_exporters.cpp
    ${editor_test_engine_dir}/imgui_te_perftool.cpp
    ${editor_test_engine_dir}/imgui_te_ui.cpp
    ${editor_test_engine_dir}/imgui_te_utils.cpp
)
target_include_directories(near_laugh_imgui_automation SYSTEM PUBLIC
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${editor_test_engine_dir})
target_compile_definitions(near_laugh_imgui_automation PUBLIC
    IMGUI_USER_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/src/editor/automation/imgui_config.hpp"
    NEAR_LAUGH_UI_AUTOMATION=1)
target_link_libraries(near_laugh_imgui_automation PRIVATE Vulkan::Vulkan glfw shell32)

add_library(near_laugh_editor_automation STATIC
    src/editor/automation/engine_session.cpp src/editor/automation/protocol.cpp
    src/editor/automation/channel.cpp src/editor/automation/application_snapshot.cpp
    src/editor/automation/session.cpp src/editor/automation/semantic_ui.cpp
    src/editor/automation/widgets.cpp)
target_include_directories(near_laugh_editor_automation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    "${CMAKE_BINARY_DIR}/generated/editor-automation")
target_link_libraries(near_laugh_editor_automation PRIVATE near_laugh_imgui_automation
    nlohmann_json::nlohmann_json near_laugh_editor_core)
near_laugh_enable_warnings(near_laugh_editor_automation)

# Recompile the real workspace/presentation with one consistent ImGui config.
get_target_property(editor_ui_sources near_laugh_editor_ui SOURCES)
add_library(near_laugh_editor_ui_automation STATIC ${editor_ui_sources})
target_include_directories(near_laugh_editor_ui_automation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(near_laugh_editor_ui_automation PRIVATE
    near_laugh_editor_core near_laugh_platform near_laugh_world
    near_laugh_imgui_automation near_laugh_text glfw nlohmann_json::nlohmann_json)
near_laugh_enable_warnings(near_laugh_editor_ui_automation)

get_target_property(editor_render_sources near_laugh_editor_render SOURCES)
add_library(near_laugh_editor_render_automation STATIC ${editor_render_sources})
target_include_directories(near_laugh_editor_render_automation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(near_laugh_editor_render_automation PRIVATE
    near_laugh_render near_laugh_platform near_laugh_world
    near_laugh_imgui_automation Vulkan::Vulkan)
target_compile_definitions(near_laugh_editor_render_automation PRIVATE
    $<$<CONFIG:Debug>:NEAR_LAUGH_ENABLE_VULKAN_VALIDATION=1>)
near_laugh_enable_warnings(near_laugh_editor_render_automation)

get_target_property(editor_application_sources level_editor SOURCES)
list(REMOVE_ITEM editor_application_sources src/editor/main.cpp)
add_executable(level_editor_automation ${editor_application_sources}
    src/editor/automation/main.cpp)
target_include_directories(level_editor_automation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(level_editor_automation PRIVATE
    near_laugh_editor_core near_laugh_editor_ui_automation near_laugh_editor_render_automation
    near_laugh_platform near_laugh_world near_laugh_audio near_laugh_imgui_automation
    near_laugh_editor_automation nlohmann_json::nlohmann_json)
near_laugh_enable_warnings(level_editor_automation)
set_target_properties(level_editor_automation PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_dependencies(level_editor_automation near_laugh_packaged_resources)
add_custom_command(TARGET level_editor_automation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:level_editor_automation>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${editor_test_engine_dir}/LICENSE.txt"
        "$<TARGET_FILE_DIR:level_editor_automation>/licenses/imgui-test-engine.txt"
    VERBATIM)

if(BUILD_TESTING)
    add_executable(editor_automation_input_probe ${editor_application_sources}
        tests/automation/native_input_probe.cpp)
    target_include_directories(editor_automation_input_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(editor_automation_input_probe PRIVATE
        near_laugh_editor_core near_laugh_editor_ui_automation near_laugh_editor_render_automation
        near_laugh_platform near_laugh_world near_laugh_audio near_laugh_imgui_automation
        near_laugh_editor_automation nlohmann_json::nlohmann_json glfw)
    near_laugh_enable_warnings(editor_automation_input_probe)
    set_target_properties(editor_automation_input_probe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_dependencies(editor_automation_input_probe near_laugh_packaged_resources)
    add_test(NAME editor_automation_input_isolation COMMAND editor_automation_input_probe)
    set_tests_properties(editor_automation_input_isolation PROPERTIES
        LABELS "vulkan-smoke" TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(editor_automation_dependency_probe tests/automation/dependency_probe.cpp)
    target_link_libraries(editor_automation_dependency_probe PRIVATE near_laugh_imgui_automation)
    near_laugh_enable_warnings(editor_automation_dependency_probe)
    add_test(NAME editor_automation_dependency_probe COMMAND editor_automation_dependency_probe)
    set_tests_properties(editor_automation_dependency_probe PROPERTIES LABELS "unit" TIMEOUT 30)
    add_executable(editor_automation_tests tests/automation/test_engine_session.cpp
        tests/automation/test_protocol.cpp tests/automation/test_application_snapshot.cpp
        tests/automation/test_session.cpp
        tests/automation/test_commit_policy.cpp
        tests/automation/test_semantic_ui.cpp
        src/launcher/executable_path.cpp)
    target_include_directories(editor_automation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(editor_automation_tests PRIVATE
        near_laugh_editor_automation near_laugh_editor_ui_automation near_laugh_imgui_automation GTest::gtest_main
        nlohmann_json::nlohmann_json)
    near_laugh_enable_warnings(editor_automation_tests)
    include(GoogleTest)
    gtest_discover_tests(editor_automation_tests
        TEST_PREFIX "automation." WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        PROPERTIES LABELS editor-automation TIMEOUT 120)
endif()
