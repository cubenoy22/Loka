function(loka_validate_standalone_performance_runs runs)
  if(NOT "${runs}" MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
      "LOKA_STANDALONE_PERFORMANCE_RUNS must be a decimal integer between 3 and 10; got '${runs}'")
  endif()
  if(runs LESS 3 OR runs GREATER 10)
    message(FATAL_ERROR
      "LOKA_STANDALONE_PERFORMANCE_RUNS must be between 3 and 10; got '${runs}'")
  endif()
endfunction()

function(loka_enable_standalone_performance target source)
  if(LOKA_STANDALONE_PERFORMANCE_RUNS STREQUAL "")
    return()
  endif()

  loka_validate_standalone_performance_runs(
    "${LOKA_STANDALONE_PERFORMANCE_RUNS}")
  target_sources(${target} PRIVATE "${source}")
  target_compile_definitions(${target} PRIVATE
    LOKA_STANDALONE_PERFORMANCE_RUNS=${LOKA_STANDALONE_PERFORMANCE_RUNS})
endfunction()
