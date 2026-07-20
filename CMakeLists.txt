cmake_minimum_required(VERSION 3.15)
project(Loka LANGUAGES CXX)

option(LOKA_ENABLE_APPLE "Enable Apple platform targets" ON)
option(LOKA_MACOS_EXPLICIT_NO_ARC_FLAGS "Add explicit -fno-objc-arc flags to macOS example targets" ON)
option(LOKA_MACOS_SUPPRESS_ARCLITE_LINK "Remove libarclite from generated macOS link settings" OFF)
option(TEST_BUILD "Enable test build mode" OFF)
option(LOKA_LIFECYCLE_AUDIT "Enable lifecycle alive-counter auditing" OFF)
option(LOKA_RETRO68_DIAGNOSTICS
  "Diagnostic Retro68 profile: keep MacsBug names, the function-tick profiler, and debug-stats dumps" OFF)

macro(loka_suppress_arclite_link)
  if(LOKA_MACOS_SUPPRESS_ARCLITE_LINK)
    foreach(_loka_implicit_link_var
        CMAKE_CXX_IMPLICIT_LINK_LIBRARIES
        CMAKE_OBJCXX_IMPLICIT_LINK_LIBRARIES)
      if(DEFINED ${_loka_implicit_link_var})
        list(FILTER ${_loka_implicit_link_var} EXCLUDE REGEX ".*arclite.*")
      endif()
    endforeach()
  endif()
endmacro()

set(LOKA_CLASSIC_MAC OFF)
if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  set(LOKA_CLASSIC_MAC ON)
endif()

# Compact vs diagnostic Classic profile (#135). Compact (the default) ships
# without the function-tick profiler and without the debug-stats dump chain;
# the diagnostic profile keeps both plus MacsBug names. add_definitions so
# the macros stay uniform across LokaAppleToolboxCore and the app targets
# (PROFILE_FUNC expands inside shared headers).
if(LOKA_CLASSIC_MAC AND LOKA_RETRO68_DIAGNOSTICS)
  add_definitions(-DLOKA_RETRO68_DIAGNOSTICS=1 -DLOKA_PROFILE_FUNC_TICKS=1)
endif()

set(LOKA_CLASSIC_TARGET_SUFFIX "")
if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
  set(LOKA_CLASSIC_TARGET_SUFFIX "68K")
elseif(CMAKE_SYSTEM_NAME MATCHES "^Retro(PPC|Carbon)$")
  set(LOKA_CLASSIC_TARGET_SUFFIX "PPC")
endif()

if(TEST_BUILD)
  add_definitions(-DTEST_BUILD)
endif()

if(LOKA_LIFECYCLE_AUDIT)
  add_definitions(-DLOKA_LIFECYCLE_AUDIT)
endif()

# Top-level CTest wiring so `ctest --test-dir <build dir>` finds test targets
# registered in subdirectories (e.g. LokaTestsMacOS) as well as the headless
# suite below. include(CTest) rather than enable_testing() so ctest's
# DartConfiguration.tcl exists (VS Code invokes ctest with -T test). The
# property suppresses the CDash dashboard targets (Nightly/Continuous/...)
# that would otherwise clutter generated Xcode/MSVC projects.
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LokaTestSources.cmake)

if(WIN32)
  add_subdirectory(win32)
endif()

if(APPLE AND LOKA_ENABLE_APPLE)
  add_subdirectory(apple)
endif()

if(LOKA_CLASSIC_MAC)
  add_subdirectory(apple/common/carbon)
  add_subdirectory(apple/toolbox)
endif()

if(WIN32 OR (APPLE AND LOKA_ENABLE_APPLE) OR LOKA_CLASSIC_MAC)
add_subdirectory(example/HelloWorld)
add_subdirectory(example/MineSweeper)
add_subdirectory(example/SimpleViewer)
add_subdirectory(example/FloppyBird)
add_subdirectory(example/Tutorial)
endif()

# Linux/WSL headless tests.
if(NOT WIN32 AND NOT APPLE AND NOT LOKA_CLASSIC_MAC)
  file(GLOB_RECURSE LOKA_COMMON_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/common/*.cpp
  )

  add_library(LokaCommonTesting STATIC
    ${LOKA_COMMON_SOURCES}
  )
  target_include_directories(LokaCommonTesting PUBLIC
    ${PROJECT_SOURCE_DIR}/common
  )
  set_target_properties(LokaCommonTesting PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  target_compile_definitions(LokaCommonTesting PRIVATE TEST_BUILD)

  set(LOKA_CONTRACT_TEST_SOURCES ${LOKA_SHARED_TEST_SOURCES})
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/FlowDslTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/FlowDslTestMain\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/example/HelloWorld/src/MainNode\\.cpp$")

  add_executable(LokaContractTests
    ${PROJECT_SOURCE_DIR}/tests/ContractTestMain.cpp
    ${LOKA_CONTRACT_TEST_SOURCES}
  )
  target_include_directories(LokaContractTests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
  )
  set_target_properties(LokaContractTests PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  target_compile_definitions(LokaContractTests PRIVATE TEST_BUILD)
  target_link_libraries(LokaContractTests PRIVATE LokaCommonTesting)

  add_executable(LokaScenarioTests
    ${PROJECT_SOURCE_DIR}/tests/ScenarioTestMain.cpp
    ${PROJECT_SOURCE_DIR}/tests/FlowDslTests.cpp
    ${PROJECT_SOURCE_DIR}/example/HelloWorld/src/MainNode.cpp
  )
  target_include_directories(LokaScenarioTests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/example/SimpleViewer/src
    ${PROJECT_SOURCE_DIR}/example/HelloWorld/src
  )
  set_target_properties(LokaScenarioTests PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  target_compile_definitions(LokaScenarioTests PRIVATE TEST_BUILD)
  target_link_libraries(LokaScenarioTests PRIVATE LokaCommonTesting)

  # Allocation ratchet (tests/alloc-ratchet, grown from probe/zero-alloc-pin).
  # Its own binary because it replaces the global operator new/delete to
  # census heap allocations; that replacement must not perturb the other
  # test executables. Asserts steady-state per-interaction allocations stay
  # under a fixed budget (a ceiling, not a target); prints the per-stack
  # census only when the ratchet trips.
  add_executable(LokaAllocPinTests
    ${PROJECT_SOURCE_DIR}/tests/AllocPinMain.cpp
    ${PROJECT_SOURCE_DIR}/tests/AllocPinTests.cpp
    ${PROJECT_SOURCE_DIR}/example/HelloWorld/src/MainNode.cpp
  )
  target_include_directories(LokaAllocPinTests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/example/HelloWorld/src
  )
  set_target_properties(LokaAllocPinTests PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  target_compile_definitions(LokaAllocPinTests PRIVATE TEST_BUILD)
  target_link_libraries(LokaAllocPinTests PRIVATE LokaCommonTesting)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Export symbols so backtrace_symbols can attribute census stacks.
    target_link_options(LokaAllocPinTests PRIVATE -rdynamic)
  endif()

  function(loka_add_registered_test test_line target)
    string(REGEX REPLACE
      "^[ \t]*LOKA_TEST\\(([A-Za-z_][A-Za-z0-9_:]*)\\)[ \t]*$"
      "\\1" _loka_test_name "${test_line}")
    string(REGEX REPLACE "[^A-Za-z0-9_.-]" "_"
      _loka_test_work_name "${_loka_test_name}")
    set(_loka_test_work_dir
      "${CMAKE_CURRENT_BINARY_DIR}/test-work/${_loka_test_work_name}")
    file(MAKE_DIRECTORY "${_loka_test_work_dir}")
    add_test(NAME "${_loka_test_name}"
      COMMAND "$<TARGET_FILE:${target}>" "${_loka_test_name}")
    set_tests_properties("${_loka_test_name}" PROPERTIES
      WORKING_DIRECTORY "${_loka_test_work_dir}")
  endfunction()

  # The registries are configure inputs: editing a LOKA_TEST line must
  # regenerate CTestTestfile.cmake, not just rebuild the runner binaries.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/tests/ContractTestRegistry.inc
    ${PROJECT_SOURCE_DIR}/tests/ScenarioTestRegistry.inc
  )

  file(STRINGS ${PROJECT_SOURCE_DIR}/tests/ContractTestRegistry.inc
    _loka_contract_test_lines
    REGEX "^[ \t]*(LOKA_TEST\\([A-Za-z_][A-Za-z0-9_:]*\\)|LOKA_SCENARIO_TESTS_HERE\\(\\))[ \t]*$")
  file(STRINGS ${PROJECT_SOURCE_DIR}/tests/ScenarioTestRegistry.inc
    _loka_scenario_test_lines
    REGEX "^[ \t]*LOKA_TEST\\([A-Za-z_][A-Za-z0-9_:]*\\)[ \t]*$")

  foreach(_loka_contract_test_line IN LISTS _loka_contract_test_lines)
    if(_loka_contract_test_line MATCHES "LOKA_SCENARIO_TESTS_HERE")
      foreach(_loka_scenario_test_line IN LISTS _loka_scenario_test_lines)
        loka_add_registered_test("${_loka_scenario_test_line}" LokaScenarioTests)
      endforeach()
    else()
      loka_add_registered_test("${_loka_contract_test_line}" LokaContractTests)
    endif()
  endforeach()

  # The pin binary runs whole (single scenario), matching the per-test
  # working-directory convention of the registered suites.
  set(_loka_allocpin_work_dir
    "${CMAKE_CURRENT_BINARY_DIR}/test-work/testZeroAllocationPin")
  file(MAKE_DIRECTORY "${_loka_allocpin_work_dir}")
  add_test(NAME testZeroAllocationPin
    COMMAND "$<TARGET_FILE:LokaAllocPinTests>")
  set_tests_properties(testZeroAllocationPin PROPERTIES
    WORKING_DIRECTORY "${_loka_allocpin_work_dir}")
endif()
