add_executable(engine_tests
    core/test_input.cpp
    core/test_shader_provider.cpp
    core/test_vertex_layout.cpp
    core/test_vulkan_utils.cpp
    core/test_ownership.cpp
)
target_link_libraries(engine_tests PRIVATE engine_lib GTest::gtest_main)
near_laugh_enable_warnings(engine_tests)

include(GoogleTest)
gtest_discover_tests(engine_tests
    DISCOVERY_TIMEOUT 60
    XML_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test-reports"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PROPERTIES LABELS "unit" TIMEOUT 120
)

add_executable(vulkan_smoke core/vulkan_smoke.cpp)
target_link_libraries(vulkan_smoke PRIVATE engine_lib)
near_laugh_enable_warnings(vulkan_smoke)
set_target_properties(vulkan_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_custom_command(TARGET vulkan_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources/shaders"
            "$<TARGET_FILE_DIR:vulkan_smoke>/resources/shaders"
)
add_test(NAME vulkan_smoke COMMAND vulkan_smoke)
set_tests_properties(vulkan_smoke PROPERTIES
    LABELS "vulkan-smoke"
    TIMEOUT 120
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_smoke>"
)
