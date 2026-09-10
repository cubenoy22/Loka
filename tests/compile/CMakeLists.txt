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

loka_compile_pin(bindings-need-the-token
  REFUSES pins/refuses_bindings_need_the_token.cpp
  ACCEPTS pins/accepts_bindings_need_the_token.cpp
)

loka_compile_pin(keyed-declarer-is-a-member-of-the-boundary
  REFUSES pins/refuses_keyed_declarer_member.cpp
  ACCEPTS pins/accepts_keyed_declarer_member.cpp
)

loka_compile_pin(keyed-declarer-owner-is-a-boundary
  REFUSES pins/refuses_keyed_declarer_not_a_boundary.cpp
  ACCEPTS pins/accepts_keyed_declarer_member.cpp
)

loka_compile_pin(observablelist-copy
  REFUSES pins/refuses_observablelist_copy.cpp
  ACCEPTS pins/accepts_observablelist_copy.cpp
)

loka_compile_pin(observablelist-assign
  REFUSES pins/refuses_observablelist_assign.cpp
  ACCEPTS pins/accepts_observablelist_assign.cpp
)

loka_compile_pin(mirroredlist-copy
  REFUSES pins/refuses_mirroredlist_copy.cpp
  ACCEPTS pins/accepts_mirroredlist_copy.cpp
)

loka_compile_pin(mirroredlist-assign
  REFUSES pins/refuses_mirroredlist_assign.cpp
  ACCEPTS pins/accepts_mirroredlist_assign.cpp
)

loka_compile_pin(lazyscope-requires-owning-node
  REFUSES pins/refuses_lazyscope_component.cpp
  ACCEPTS pins/accepts_lazyscope_owner.cpp
)

loka_compile_pin(lazyflex-requires-component-props
  REFUSES pins/refuses_lazyflex_component_props.cpp
  ACCEPTS pins/accepts_lazyflex_component_props.cpp
)

loka_compile_pin(tracker-copy
  REFUSES pins/refuses_tracker_copy.cpp
  ACCEPTS pins/accepts_tracker_construction.cpp
)

loka_compile_pin(tracker-assign
  REFUSES pins/refuses_tracker_assign.cpp
  ACCEPTS pins/accepts_tracker_construction.cpp
)
