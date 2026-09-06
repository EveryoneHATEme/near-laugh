add_executable(engine_tests
    core/test_scene_authoring.cpp
    core/test_doors.cpp
    core/test_editor_playtest.cpp
    core/test_interior_level.cpp
    core/test_editor_bridge.cpp
    core/test_editor_camera.cpp
    core/test_editor_commands.cpp
    core/test_editor_terrain.cpp
    core/test_editor_picking.cpp
    core/test_editor_ui.cpp
    core/test_editor_document.cpp
    core/test_editor_loop.cpp
    core/test_input.cpp
    core/test_level_document.cpp
    core/test_light_switch.cpp
    core/test_executable_path.cpp
    core/test_frame.cpp
    core/test_fixed_step.cpp
    core/test_player_flashlight.cpp
    core/test_runtime_resources.cpp
    core/test_validation_diagnostics.cpp
    core/test_shader_provider.cpp
    core/test_static_model_loader.cpp
    core/test_texture_assets.cpp
    core/test_vertex_layout.cpp
    core/test_world.cpp
    core/test_vulkan_utils.cpp
    core/test_ownership.cpp
    core/test_physics.cpp
    core/test_player_controller.cpp
    core/test_prototype_scene.cpp
    ${CMAKE_SOURCE_DIR}/src/launcher/executable_path.cpp
    ${CMAKE_SOURCE_DIR}/src/launcher/launch_options.cpp
)
target_include_directories(engine_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(engine_tests PRIVATE
    near_laugh_runtime near_laugh_platform near_laugh_world near_laugh_physics
    near_laugh_render near_laugh_editor_core near_laugh_editor_ui
    near_laugh_editor_render
    Vulkan::Vulkan near_laugh_imgui GTest::gtest_main
)
near_laugh_enable_warnings(engine_tests)

add_executable(editor_argument_probe process/editor_argument_probe.cpp)
near_laugh_enable_warnings(editor_argument_probe)
add_dependencies(engine_tests editor_argument_probe)
target_compile_definitions(engine_tests PRIVATE
    EDITOR_ARGUMENT_PROBE_PATH="$<TARGET_FILE:editor_argument_probe>")

include(GoogleTest)
gtest_discover_tests(engine_tests
    DISCOVERY_TIMEOUT 60
    XML_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test-reports"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PROPERTIES LABELS "unit" TIMEOUT 120
)

add_executable(vulkan_smoke core/vulkan_smoke.cpp)
target_include_directories(vulkan_smoke PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(vulkan_smoke PRIVATE
    near_laugh_runtime near_laugh_platform near_laugh_render Vulkan::Vulkan
)
near_laugh_enable_warnings(vulkan_smoke)
set_target_properties(vulkan_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_dependencies(vulkan_smoke near_laugh_packaged_resources)
add_test(NAME vulkan_smoke COMMAND vulkan_smoke)
set_tests_properties(vulkan_smoke PROPERTIES
    LABELS "vulkan-smoke"
    TIMEOUT 120
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_smoke>"
)
foreach(ENTRY IN ITEMS apartment lower-landing)
    add_test(NAME vulkan_interior_${ENTRY} COMMAND vulkan_smoke --interior ${ENTRY})
    set_tests_properties(vulkan_interior_${ENTRY} PROPERTIES
        LABELS "vulkan-smoke" TIMEOUT 120
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_smoke>")
endforeach()

add_test(NAME vulkan_smoke_validation_error
         COMMAND vulkan_smoke --inject-validation-error)
set_tests_properties(vulkan_smoke_validation_error PROPERTIES
    LABELS "vulkan-smoke"
    TIMEOUT 120
    WILL_FAIL TRUE
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_smoke>"
)

add_test(NAME vulkan_lifecycle_smoke COMMAND vulkan_smoke --lifecycle)
set_tests_properties(vulkan_lifecycle_smoke PROPERTIES
    LABELS "vulkan-smoke"
    TIMEOUT 120
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_smoke>"
)

add_executable(runtime_public_consumer boundary/minimal_consumer.cpp)
target_link_libraries(runtime_public_consumer PRIVATE near_laugh_runtime)
near_laugh_enable_warnings(runtime_public_consumer)
add_test(NAME runtime_public_consumer COMMAND runtime_public_consumer)
set_tests_properties(runtime_public_consumer PROPERTIES LABELS "boundary")

add_test(NAME public_boundary_headers
    COMMAND ${CMAKE_COMMAND}
            -DROOT=${CMAKE_SOURCE_DIR}/include
            -P ${CMAKE_CURRENT_SOURCE_DIR}/boundary/check_headers.cmake
)
set_tests_properties(public_boundary_headers PROPERTIES LABELS "boundary")

add_test(NAME public_boundary_negative
    COMMAND ${CMAKE_COMMAND}
            -DROOT=${CMAKE_CURRENT_SOURCE_DIR}/boundary/forbidden_fixture
            -P ${CMAKE_CURRENT_SOURCE_DIR}/boundary/check_headers.cmake
)

add_test(NAME public_boundary_jolt_negative
    COMMAND ${CMAKE_COMMAND}
            -DROOT=${CMAKE_CURRENT_SOURCE_DIR}/boundary/jolt_fixture
            -P ${CMAKE_CURRENT_SOURCE_DIR}/boundary/check_headers.cmake
)
set_tests_properties(public_boundary_jolt_negative PROPERTIES
    LABELS "boundary"
    WILL_FAIL TRUE
)
set_tests_properties(public_boundary_negative PROPERTIES
    LABELS "boundary"
    WILL_FAIL TRUE
)

add_test(NAME source_boundary_check
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DBUILD_ROOT=${CMAKE_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/boundary/check_sources.cmake
)
set_tests_properties(source_boundary_check PROPERTIES LABELS "boundary")

add_test(NAME editor_executable_resources
    COMMAND ${CMAKE_COMMAND}
            -DROOT=$<TARGET_FILE_DIR:level_editor>/resources
            -P ${CMAKE_CURRENT_SOURCE_DIR}/boundary/check_editor_resources.cmake
)
set_tests_properties(editor_executable_resources PROPERTIES LABELS "boundary")

add_test(NAME editor_vulkan_smoke COMMAND level_editor --smoke)
set_tests_properties(editor_vulkan_smoke PROPERTIES
    LABELS "vulkan-smoke"
    TIMEOUT 120
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:level_editor>"
)

add_test(NAME editor_vulkan_construction_failure COMMAND level_editor --smoke)
set_tests_properties(editor_vulkan_construction_failure PROPERTIES
    LABELS "vulkan-smoke"
    TIMEOUT 120
    WILL_FAIL TRUE
    ENVIRONMENT "NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE=editor-imgui-backend"
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:level_editor>"
)

add_executable(resource_layout_probe
    process/resource_layout_probe.cpp
    ${CMAKE_SOURCE_DIR}/src/launcher/executable_path.cpp
)
target_include_directories(resource_layout_probe PRIVATE "${CMAKE_SOURCE_DIR}/src")
near_laugh_enable_warnings(resource_layout_probe)
set_target_properties(resource_layout_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_dependencies(resource_layout_probe near_laugh_packaged_resources)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/resource-layout-working")
add_test(NAME executable_relative_resource_layout COMMAND resource_layout_probe)
add_test(NAME game_launch_option_failures
    COMMAND ${CMAKE_COMMAND} -DGAME=$<TARGET_FILE:near_laugh>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/process/check_launch_options.cmake)
set_tests_properties(game_launch_option_failures PROPERTIES LABELS "process")
set_tests_properties(executable_relative_resource_layout PROPERTIES
    LABELS "process"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests/resource-layout-working"
)
