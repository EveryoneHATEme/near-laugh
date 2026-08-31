add_executable(engine_tests
    core/test_input.cpp
    core/test_executable_path.cpp
    core/test_frame.cpp
    core/test_fixed_step.cpp
    core/test_prototype_rifle.cpp
    core/test_shooting_targets.cpp
    core/test_shooting_range.cpp
    core/test_runtime_resources.cpp
    core/test_validation_diagnostics.cpp
    core/test_shader_provider.cpp
    core/test_texture_assets.cpp
    core/test_vertex_layout.cpp
    core/test_world.cpp
    core/test_vulkan_utils.cpp
    core/test_ownership.cpp
    core/test_physics.cpp
    core/test_player_controller.cpp
    core/test_prototype_scene.cpp
    ${CMAKE_SOURCE_DIR}/src/launcher/executable_path.cpp
)
target_include_directories(engine_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(engine_tests PRIVATE
    near_laugh_runtime near_laugh_platform near_laugh_world near_laugh_physics
    near_laugh_render
    Vulkan::Vulkan GTest::gtest_main
)
near_laugh_enable_warnings(engine_tests)

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
add_custom_command(TARGET vulkan_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources/shaders"
            "$<TARGET_FILE_DIR:vulkan_smoke>/resources/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources/textures"
            "$<TARGET_FILE_DIR:vulkan_smoke>/resources/textures"
)
add_test(NAME vulkan_smoke COMMAND vulkan_smoke)
set_tests_properties(vulkan_smoke PROPERTIES
    LABELS "vulkan-smoke"
    TIMEOUT 120
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_smoke>"
)

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

add_executable(resource_layout_probe
    process/resource_layout_probe.cpp
    ${CMAKE_SOURCE_DIR}/src/launcher/executable_path.cpp
)
target_include_directories(resource_layout_probe PRIVATE "${CMAKE_SOURCE_DIR}/src")
near_laugh_enable_warnings(resource_layout_probe)
set_target_properties(resource_layout_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_custom_command(TARGET resource_layout_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources/shaders"
            "$<TARGET_FILE_DIR:resource_layout_probe>/resources/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources/textures"
            "$<TARGET_FILE_DIR:resource_layout_probe>/resources/textures"
)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/resource-layout-working")
add_test(NAME executable_relative_resource_layout COMMAND resource_layout_probe)
set_tests_properties(executable_relative_resource_layout PROPERTIES
    LABELS "process"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests/resource-layout-working"
)
