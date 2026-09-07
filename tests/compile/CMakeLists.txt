# loka_compile_pin(<name> REFUSES <tu> ACCEPTS <tu>)
function(loka_compile_pin NAME)
  cmake_parse_arguments(PIN "" "REFUSES;ACCEPTS" "" ${ARGN})
  if(NOT PIN_REFUSES OR NOT PIN_ACCEPTS OR PIN_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "loka_compile_pin requires REFUSES <tu> ACCEPTS <tu>")
  endif()

  add_library(compilePin_${NAME}_refuses OBJECT EXCLUDE_FROM_ALL ${PIN_REFUSES})
  add_library(compilePin_${NAME}_accepts OBJECT ${PIN_ACCEPTS})
  foreach(kind IN ITEMS refuses accepts)
    set(target compilePin_${NAME}_${kind})
    target_link_libraries(${target} PRIVATE LokaContractTestEnvironment)
    loka_enable_warnings(${target})
    set_target_properties(${target} PROPERTIES
      CXX_STANDARD 98
      CXX_STANDARD_REQUIRED YES
    )
  endforeach()

  if(LOKA_COMPILE_PINS)
    add_test(NAME compileRefuses-${NAME}
      COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
        --config $<CONFIG> --target compilePin_${NAME}_refuses)
    set_tests_properties(compileRefuses-${NAME} PROPERTIES
      WILL_FAIL TRUE RESOURCE_LOCK loka-build-dir)
  endif()
  add_test(NAME compileAccepts-${NAME}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
      --config $<CONFIG> --target compilePin_${NAME}_accepts)
  set_tests_properties(compileAccepts-${NAME} PROPERTIES
    RESOURCE_LOCK loka-build-dir)
endfunction()

loka_compile_pin(borrowed-state-is-read-only
  REFUSES pins/refuses_set_on_borrowed_state.cpp
  ACCEPTS pins/accepts_get_on_borrowed_state.cpp
)

loka_compile_pin(local-recompose-doors-are-internal
  REFUSES pins/refuses_local_recompose_doors.cpp
  ACCEPTS pins/accepts_local_recompose_doors.cpp
)
