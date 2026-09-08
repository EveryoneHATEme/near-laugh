file(MAKE_DIRECTORY "${WORK}/isolated-character/bin/resources")
file(COPY "${VIEWER}" DESTINATION "${WORK}/isolated-character/bin")
file(COPY "${RESOURCES}/characters" DESTINATION "${WORK}/isolated-character/bin/resources")
get_filename_component(VIEWER_NAME "${VIEWER}" NAME)
execute_process(COMMAND "${WORK}/isolated-character/bin/${VIEWER_NAME}" --preflight
    WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE RESULT OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR)
if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "Isolated prepared-character preflight failed: ${OUTPUT}${ERROR}")
endif()
