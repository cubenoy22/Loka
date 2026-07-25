option(LOKA_WARNINGS_AS_ERRORS
  "Treat Loka target compiler warnings as errors" OFF)

# Applies Loka's compiler-warning floor to one repository-owned target.
# Keep this target-local: platform SDKs and future third-party targets must
# not inherit Loka's warning policy through directory-wide compiler flags.
function(loka_enable_warnings target)
  if(MSVC)
    # C4458 reports deliberate API vocabulary such as context, diff, width,
    # and height shadowing qualified members. Renaming those parameters would
    # make cross-layer contracts less consistent without preventing a bug.
    # C4996 rejects portable C stdio in favor of MSVC-only secure CRT APIs;
    # Loka deliberately shares C++98/C stdio code with Classic targets.
    target_compile_options(${target} PRIVATE /W4 /wd4458 /wd4996)
    if(LOKA_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra)

    # GCC 11+ diagnoses every Node new-expression because Node supplies a
    # class-specific delete for arena storage. The ownership hole is real but
    # currently unreachable; issue #175 owns the structural fix. Waive only
    # that diagnostic until the three Node allocation doors become explicit.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
       CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11)
      target_compile_options(${target} PRIVATE -Wno-mismatched-new-delete)
    endif()

    if(LOKA_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()

  set_property(TARGET ${target} PROPERTY LOKA_WARNING_FLOOR_ENABLED TRUE)
endfunction()

function(_loka_verify_warning_floor_in_directory directory)
  get_property(_loka_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_loka_target IN LISTS _loka_targets)
    get_target_property(_loka_target_type ${_loka_target} TYPE)
    if(_loka_target MATCHES "^Loka" AND
       NOT _loka_target_type STREQUAL "INTERFACE_LIBRARY" AND
       NOT _loka_target_type STREQUAL "UTILITY")
      get_target_property(_loka_floor_enabled ${_loka_target} LOKA_WARNING_FLOOR_ENABLED)
      if(NOT _loka_floor_enabled)
        message(FATAL_ERROR
          "Loka target ${_loka_target} has no compiler-warning floor; call loka_enable_warnings(${_loka_target})")
      endif()

      get_target_property(_loka_compile_options ${_loka_target} COMPILE_OPTIONS)
      if(MSVC)
        list(FIND _loka_compile_options "/W4" _loka_baseline_index)
        set(_loka_error_flag "/WX")
      elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        list(FIND _loka_compile_options "-Wall" _loka_baseline_index)
        list(FIND _loka_compile_options "-Wextra" _loka_extra_index)
        if(_loka_extra_index EQUAL -1)
          message(FATAL_ERROR "Loka target ${_loka_target} is missing -Wextra")
        endif()
        set(_loka_error_flag "-Werror")
      else()
        set(_loka_baseline_index 0)
        set(_loka_error_flag "")
      endif()
      if(_loka_baseline_index EQUAL -1)
        message(FATAL_ERROR "Loka target ${_loka_target} is missing its baseline warning flag")
      endif()
      if(LOKA_WARNINGS_AS_ERRORS AND _loka_error_flag)
        list(FIND _loka_compile_options "${_loka_error_flag}" _loka_error_index)
        if(_loka_error_index EQUAL -1)
          message(FATAL_ERROR "Loka target ${_loka_target} is missing ${_loka_error_flag}")
        endif()
      endif()
    endif()
  endforeach()

  get_property(_loka_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(_loka_subdirectory IN LISTS _loka_subdirectories)
    _loka_verify_warning_floor_in_directory("${_loka_subdirectory}")
  endforeach()
endfunction()

function(loka_verify_warning_floor)
  _loka_verify_warning_floor_in_directory("${CMAKE_SOURCE_DIR}")
endfunction()
