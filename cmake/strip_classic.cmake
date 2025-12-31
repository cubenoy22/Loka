if(NOT DEFINED STRIP_TOOL OR NOT STRIP_TOOL)
  message(FATAL_ERROR "STRIP_TOOL is required.")
endif()

if(NOT DEFINED STRIP_INPUT OR NOT STRIP_INPUT)
  message(FATAL_ERROR "STRIP_INPUT is required.")
endif()

if(EXISTS "${STRIP_INPUT}")
  execute_process(
    COMMAND "${STRIP_TOOL}" "${STRIP_INPUT}"
    RESULT_VARIABLE strip_result
  )
  if(NOT strip_result EQUAL 0)
    message(WARNING "Strip failed for ${STRIP_INPUT} (code ${strip_result}).")
  endif()
else()
  message(STATUS "Strip skipped; file not found: ${STRIP_INPUT}")
endif()
