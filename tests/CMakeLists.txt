cmake_policy(SET CMP0135 NEW)

file(GLOB_RECURSE TEST_SOURCES 
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.cpp"
)

add_executable(engine_tests ${TEST_SOURCES})

target_link_libraries(engine_tests PRIVATE
    engine_lib
    ${ENGINE_LIB_LIBS}
    GTest::gtest_main
)

target_include_directories(engine_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    add_custom_command(TARGET engine_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:engine_tests>"
        "$<TARGET_FILE_DIR:engine_tests>"
        COMMAND_EXPAND_LISTS
    )
endif()

include(GoogleTest)
gtest_discover_tests(engine_tests
    DISCOVERY_TIMEOUT 60
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-reports
    PROPERTIES
        LABELS "unit"
        TIMEOUT 120
)