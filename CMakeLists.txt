cmake_minimum_required(VERSION 3.15)
project(Loka VERSION 0.0.3 LANGUAGES CXX)

option(LOKA_ENABLE_APPLE "Enable Apple platform targets" ON)
option(LOKA_MACOS_EXPLICIT_NO_ARC_FLAGS "Add explicit -fno-objc-arc flags to macOS example targets" ON)
option(LOKA_MACOS_SUPPRESS_ARCLITE_LINK "Remove libarclite from generated macOS link settings" OFF)
option(TEST_BUILD "Enable test build mode" OFF)
option(LOKA_MAME_SCENARIOS "Register the MAME-backed scenario tests" OFF)
option(LOKA_WIN32_SCENARIOS "Register the rig-local Win32 scenario tests" OFF)
option(LOKA_LIFECYCLE_AUDIT "Enable lifecycle alive-counter auditing" OFF)
option(LOKA_RETRO68_DIAGNOSTICS
  "Diagnostic Retro68 profile: keep MacsBug names, the function-tick profiler, and debug-stats dumps" OFF)
option(LOKA_TOOLBOX_MULTIVERSAL_INTERFACES
  "Build Toolbox targets against Retro68's redistributable Multiversal Interfaces" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LokaWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LokaStandalonePerformance.cmake)

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

set(LOKA_MACOS OFF)
if(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(LOKA_MACOS ON)
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
  add_definitions(-DTEST_BUILD -DLOKA_DIAG=1)
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

foreach(_loka_invalid_performance_runs IN ITEMS abc ON 0 2 11)
  add_test(NAME "standalonePerformanceConfigRejects-${_loka_invalid_performance_runs}"
    COMMAND ${CMAKE_COMMAND}
      -DLOKA_REPOSITORY_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -DLOKA_TEST_PERFORMANCE_RUNS=${_loka_invalid_performance_runs}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/StandalonePerformanceConfigProbe.cmake)
  set_tests_properties(
    "standalonePerformanceConfigRejects-${_loka_invalid_performance_runs}"
    PROPERTIES WILL_FAIL TRUE)
endforeach()
unset(_loka_invalid_performance_runs)
add_test(NAME standalonePerformanceConfigAccepts-3
  COMMAND ${CMAKE_COMMAND}
    -DLOKA_REPOSITORY_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -DLOKA_TEST_PERFORMANCE_RUNS=3
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/StandalonePerformanceConfigProbe.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LokaTestSources.cmake)

# Compile-only regression pins are built by the headless test profile and by
# the Classic diagnostic profile, but are never linked into shipped binaries.
if(TEST_BUILD OR (LOKA_CLASSIC_MAC AND LOKA_RETRO68_DIAGNOSTICS))
  add_library(LokaProfilerCompileTest OBJECT
    ${PROJECT_SOURCE_DIR}/tests/ProfilerCompileTest.cpp
  )
  loka_enable_warnings(LokaProfilerCompileTest)
  target_include_directories(LokaProfilerCompileTest PRIVATE
    ${PROJECT_SOURCE_DIR}/common
  )
  target_compile_definitions(LokaProfilerCompileTest PRIVATE
    LOKA_PROFILE_FUNC_TICKS=1
  )
  set_target_properties(LokaProfilerCompileTest PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
endif()

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

if(WIN32 OR (LOKA_MACOS AND LOKA_ENABLE_APPLE) OR LOKA_CLASSIC_MAC)
add_subdirectory(example/HelloWorld)
add_subdirectory(example/MineSweeper)
add_subdirectory(example/SimpleViewer)
add_subdirectory(example/ScrapbookUI)
add_subdirectory(example/FloppyBird)
add_subdirectory(example/Tutorial)
endif()

if(NOT IOS)
  add_subdirectory(tests/toolbox)
endif()

# Linux/WSL headless tests.
if(NOT WIN32 AND NOT APPLE AND NOT LOKA_CLASSIC_MAC)
  file(GLOB_RECURSE LOKA_COMMON_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/common/*.cpp
  )

  # The shipping compile. Every Linux target above defines TEST_BUILD, so the
  # branches that only exist without it -- MenuBarDefinition::clone()'s
  # failure-injection gate, the TEST_ID paths -- were never compiled on this
  # platform at all. This object library compiles common/ the way a shipped
  # binary sees it, under the same warnings-as-errors floor. It is never
  # linked; the point is that the code is seen by a compiler.
  add_library(LokaCommonShippingCompileTest OBJECT
    ${LOKA_COMMON_SOURCES}
  )
  loka_enable_warnings(LokaCommonShippingCompileTest)
  # add_definitions(-DTEST_BUILD) is directory-scoped, so this target has to
  # take the define back off to be the shipping compile it claims to be.
  target_compile_options(LokaCommonShippingCompileTest PRIVATE
    -UTEST_BUILD
    -ULOKA_DIAG
  )
  target_include_directories(LokaCommonShippingCompileTest PRIVATE
    ${PROJECT_SOURCE_DIR}/common
    ${PROJECT_SOURCE_DIR}/tools
  )
  set_target_properties(LokaCommonShippingCompileTest PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  add_library(LokaCommonTesting STATIC
    ${LOKA_COMMON_SOURCES}
  )
  loka_enable_warnings(LokaCommonTesting)
  target_include_directories(LokaCommonTesting PUBLIC
    ${PROJECT_SOURCE_DIR}/common
    ${PROJECT_SOURCE_DIR}/tools
  )
  set_target_properties(LokaCommonTesting PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  target_compile_definitions(LokaCommonTesting PRIVATE TEST_BUILD)

  set(LOKA_CONTRACT_TEST_SOURCES ${LOKA_SHARED_TEST_SOURCES})
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/FlowDslTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/OwnershipDumpTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/FlowDslTestMain\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/example/HelloWorld/src/MainNode\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/HelloWorldScenarioTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/scenarios/HelloWorldScenarios\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/standalone/HelloWorldStandaloneFlowAppConfig\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/TutorialScenarioTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/scenarios/TutorialScenarios\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/standalone/TutorialStandaloneFlowAppConfig\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/MineSweeperScenarioTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/scenarios/MineSweeperScenarios\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/standalone/MineSweeperStandaloneFlowAppConfig\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/FloppyBirdScenarioTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/scenarios/FloppyBirdScenarios\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/standalone/FloppyBirdStandaloneFlowAppConfig\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/StartupScenarioTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/scenarios/StartupScenarios\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/ScenarioReelTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/StandalonePerformanceTests\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/scenarios/ScenarioReel\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/standalone/StandalonePerformance\\.cpp$")
  list(FILTER LOKA_CONTRACT_TEST_SOURCES EXCLUDE REGEX "/tests/scenarios/RectSurfaceScenarioObservation\\.cpp$")

  add_executable(LokaContractTests
    ${PROJECT_SOURCE_DIR}/tests/ContractTestMain.cpp
    ${LOKA_CONTRACT_TEST_SOURCES}
  )
  loka_enable_warnings(LokaContractTests)
  target_include_directories(LokaContractTests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/tools
  )
  set_target_properties(LokaContractTests PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  target_compile_definitions(LokaContractTests PRIVATE TEST_BUILD)
  target_link_libraries(LokaContractTests PRIVATE LokaCommonTesting)

  add_executable(LokaScenarioTests
    ${PROJECT_SOURCE_DIR}/tests/ScenarioTestMain.cpp
    ${PROJECT_SOURCE_DIR}/tests/TestingHooks.cpp
    ${PROJECT_SOURCE_DIR}/tests/FlowDslTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/OwnershipDumpTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/HelloWorldScenarioTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/ScenarioReelTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/StandalonePerformanceTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
    ${PROJECT_SOURCE_DIR}/tests/standalone/StandalonePerformance.cpp
    ${PROJECT_SOURCE_DIR}/tests/scenarios/ScenarioReel.cpp
    ${PROJECT_SOURCE_DIR}/tests/scenarios/HelloWorldScenarios.cpp
    ${PROJECT_SOURCE_DIR}/tests/TutorialScenarioTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/scenarios/TutorialScenarios.cpp
    ${PROJECT_SOURCE_DIR}/tests/standalone/TutorialStandaloneFlowAppConfig.cpp
    ${PROJECT_SOURCE_DIR}/tests/MineSweeperScenarioTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/scenarios/MineSweeperScenarios.cpp
    ${PROJECT_SOURCE_DIR}/tests/standalone/MineSweeperStandaloneFlowAppConfig.cpp
    ${PROJECT_SOURCE_DIR}/tests/FloppyBirdScenarioTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/scenarios/FloppyBirdScenarios.cpp
    ${PROJECT_SOURCE_DIR}/tests/scenarios/RectSurfaceScenarioObservation.cpp
    ${PROJECT_SOURCE_DIR}/tests/standalone/FloppyBirdStandaloneFlowAppConfig.cpp
    ${PROJECT_SOURCE_DIR}/tests/StartupScenarioTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/scenarios/StartupScenarios.cpp
    ${PROJECT_SOURCE_DIR}/tests/platform/null/NullScenePlatformController.cpp
    ${PROJECT_SOURCE_DIR}/tests/platform/null/NullPlatformContext.cpp
    ${PROJECT_SOURCE_DIR}/tests/platform/null/context/NullButtonContext.cpp
    ${PROJECT_SOURCE_DIR}/tests/platform/null/context/NullEditTextContext.cpp
    ${PROJECT_SOURCE_DIR}/tests/platform/null/context/NullScrollBarContext.cpp
    ${PROJECT_SOURCE_DIR}/tests/platform/null/context/NullTextContext.cpp
    ${PROJECT_SOURCE_DIR}/tests/standalone/HelloWorldStandaloneFlowAppConfig.cpp
    ${PROJECT_SOURCE_DIR}/tests/standalone/StandaloneScenarioSupport.cpp
    ${PROJECT_SOURCE_DIR}/example/HelloWorld/src/MainNode.cpp
  )
  loka_enable_warnings(LokaScenarioTests)
  target_include_directories(LokaScenarioTests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/example/SimpleViewer/src
    ${PROJECT_SOURCE_DIR}/example/HelloWorld/src
    ${PROJECT_SOURCE_DIR}/example/Tutorial/src
    ${PROJECT_SOURCE_DIR}/example/MineSweeper/src
    ${PROJECT_SOURCE_DIR}/example/FloppyBird/src
    ${PROJECT_SOURCE_DIR}/tools
  )
  set_target_properties(LokaScenarioTests PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  target_compile_definitions(LokaScenarioTests PRIVATE TEST_BUILD)
  target_link_libraries(LokaScenarioTests PRIVATE LokaCommonTesting)

  function(loka_add_vehicle_presentation_test target_name source_file example_include_dir test_name)
    add_executable(${target_name}
      ${PROJECT_SOURCE_DIR}/tests/scenarios/${source_file}
      ${PROJECT_SOURCE_DIR}/tests/TestingHooks.cpp
      ${PROJECT_SOURCE_DIR}/tests/platform/null/NullPlatformContext.cpp
      ${PROJECT_SOURCE_DIR}/tests/platform/null/NullScenePlatformController.cpp
      ${PROJECT_SOURCE_DIR}/tests/platform/null/context/NullButtonContext.cpp
      ${PROJECT_SOURCE_DIR}/tests/platform/null/context/NullEditTextContext.cpp
      ${PROJECT_SOURCE_DIR}/tests/platform/null/context/NullScrollBarContext.cpp
      ${PROJECT_SOURCE_DIR}/tests/platform/null/context/NullTextContext.cpp
    )
    loka_enable_warnings(${target_name})
    target_include_directories(${target_name} PRIVATE
      ${PROJECT_SOURCE_DIR}/common
      ${PROJECT_SOURCE_DIR}/tests
      ${PROJECT_SOURCE_DIR}/${example_include_dir}
    )
    set_target_properties(${target_name} PROPERTIES
      CXX_STANDARD 98
      CXX_STANDARD_REQUIRED YES
    )
    target_compile_definitions(${target_name} PRIVATE TEST_BUILD)
    target_link_libraries(${target_name} PRIVATE LokaCommonTesting)
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${target_name}>)
  endfunction()

  loka_add_vehicle_presentation_test(
    LokaTutorialVehiclePresentationTest
    TutorialVehiclePresentationTest.cpp
    example/Tutorial/src
    testTutorialVehiclePresentationUsesExampleDeclaration)
  loka_add_vehicle_presentation_test(
    LokaMineSweeperVehiclePresentationTest
    MineSweeperVehiclePresentationTest.cpp
    example/MineSweeper/src
    testMineSweeperVehiclePresentationUsesExampleDeclaration)
  loka_add_vehicle_presentation_test(
    LokaFloppyBirdVehiclePresentationTest
    FloppyBirdVehiclePresentationTest.cpp
    example/FloppyBird/src
    testFloppyBirdVehiclePresentationUsesExampleDeclaration)
  loka_add_vehicle_presentation_test(
    LokaScrapbookVehiclePresentationTest
    ScrapbookVehiclePresentationTest.cpp
    example/ScrapbookUI/src
    testScrapbookVehiclePresentationUsesExampleDeclaration)
  target_sources(LokaScrapbookVehiclePresentationTest PRIVATE
    ${PROJECT_SOURCE_DIR}/example/ScrapbookUI/src/ScrapbookPackage.cpp
  )

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
  loka_enable_warnings(LokaAllocPinTests)
  target_include_directories(LokaAllocPinTests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/example/HelloWorld/src
    ${PROJECT_SOURCE_DIR}/tools
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

  # Host-command regression fixture. This is the production lrpc entry point,
  # built only into the native test graph so scriptLrpcStageWriteFailure can
  # exercise create/write/cleanup/rename rather than a duplicated file helper.
  add_executable(LokaLrpcStageTestTool
    ${PROJECT_SOURCE_DIR}/tools/lrpc/main.cpp
    ${PROJECT_SOURCE_DIR}/tools/lrpc/HostFile.cpp
    ${PROJECT_SOURCE_DIR}/tools/lrpc/LrpkStager.cpp
    ${PROJECT_SOURCE_DIR}/tools/lrpc/PackManifest.cpp
    ${PROJECT_SOURCE_DIR}/tools/lrpc/LrpkWriter.cpp
    ${PROJECT_SOURCE_DIR}/tools/lrpc/Utf8Path.cpp
  )
  loka_enable_warnings(LokaLrpcStageTestTool)
  target_include_directories(LokaLrpcStageTestTool PRIVATE
    ${PROJECT_SOURCE_DIR}/common
    ${PROJECT_SOURCE_DIR}/tools
  )
  set_target_properties(LokaLrpcStageTestTool PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

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

  if(NOT WIN32)
    add_test(NAME scriptMameDebugLaunch
      COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/MameDebugLaunchTest.sh)
    add_test(NAME scriptMameRunLaunch
      COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/MameRunLaunchTest.sh)
    add_test(NAME scriptPngCropSelftest
      COMMAND python3 ${PROJECT_SOURCE_DIR}/tests/scenarios/pngtool.py selftest)
    add_test(NAME scriptScenarioTools
      COMMAND python3 ${PROJECT_SOURCE_DIR}/tests/scripts/ScenarioToolsTest.py)
    add_test(NAME scriptReleaseAssembler
      COMMAND python3 ${PROJECT_SOURCE_DIR}/tests/scripts/ReleaseAssemblerTest.py)
    add_test(NAME scriptPresentationStage
      COMMAND python3 ${PROJECT_SOURCE_DIR}/tests/scripts/PresentationStageTest.py)
    add_test(NAME scriptMacOSStandaloneAuditVerifier
      COMMAND python3
        ${PROJECT_SOURCE_DIR}/tests/scripts/MacOSStandaloneAuditVerifierTest.py)
    add_test(NAME scriptToolboxStandaloneStage
      COMMAND python3 ${PROJECT_SOURCE_DIR}/tests/scripts/ToolboxStandaloneStageTest.py)
    add_test(NAME scriptMacOSRigProtocol
      COMMAND python3 ${PROJECT_SOURCE_DIR}/tests/scripts/MacOSRigProtocolTest.py)
    add_test(NAME scriptLokaRig
      COMMAND python3 ${PROJECT_SOURCE_DIR}/tests/scripts/LokaRigTest.py)
    add_test(NAME scriptToolboxPresentationRail
      COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/ToolboxPresentationRailTest.sh)
    add_test(NAME scriptToolboxScenarioRunner
      COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/ToolboxScenarioRunnerTest.sh)
    add_test(NAME scriptToolboxStandaloneAuditVerifier
      COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/ToolboxStandaloneAuditVerifierTest.sh)
    add_test(NAME scriptMacOSScenarioRunner
      COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/MacOSScenarioRunnerTest.sh)
    add_test(NAME scriptBuild104Guard
      COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/Build104GuardTest.sh)
    add_test(NAME scriptMacOSBuildTargets
      COMMAND bash ${PROJECT_SOURCE_DIR}/tests/scripts/MacOSBuildTargetsTest.sh)
    add_test(NAME scriptPmonSpriteGenerator
      COMMAND python3
        ${PROJECT_SOURCE_DIR}/tests/scripts/PmonSpriteGeneratorTest.py)
    add_test(NAME scriptLrpcStageWriteFailure
      COMMAND python3
        ${PROJECT_SOURCE_DIR}/tests/scripts/LrpcStageWriteFailureTest.py
        $<TARGET_FILE:LokaLrpcStageTestTool>
        ${PROJECT_SOURCE_DIR}/example/ScrapbookUI/ASSETS.LRP)

    if(LOKA_MAME_SCENARIOS)
      file(STRINGS
        ${PROJECT_SOURCE_DIR}/tests/scenarios/scenarios.txt
        _loka_mame_scenarios
        REGEX "^[a-z0-9][a-z0-9-]* [a-z0-9][a-z0-9-]*$")
      foreach(_loka_mame_entry IN LISTS _loka_mame_scenarios)
        string(REGEX REPLACE "^([^ ]+) ([^ ]+)$" "\\1"
          _loka_mame_example "${_loka_mame_entry}")
        string(REGEX REPLACE "^([^ ]+) ([^ ]+)$" "\\2"
          _loka_mame_scenario "${_loka_mame_entry}")
        add_test(NAME "mameScenario-${_loka_mame_example}-${_loka_mame_scenario}"
          COMMAND bash ${PROJECT_SOURCE_DIR}/tests/toolbox/run-scenario.sh
            "${_loka_mame_example}" "${_loka_mame_scenario}")
        set_tests_properties("mameScenario-${_loka_mame_example}-${_loka_mame_scenario}"
          PROPERTIES LABELS "mame")
      endforeach()

      # The watchpoint legs share the fixed build/mame-debug staging area of
      # scripts/mame-debug.sh, so they must not run concurrently.
      foreach(_loka_wpset_mode clean control)
        add_test(NAME "mameScenarioWpset-${_loka_wpset_mode}"
          COMMAND bash ${PROJECT_SOURCE_DIR}/tests/toolbox/run-wpset.sh
            "${_loka_wpset_mode}")
        set_tests_properties("mameScenarioWpset-${_loka_wpset_mode}"
          PROPERTIES LABELS "mame" RUN_SERIAL TRUE)
      endforeach()
    endif()
  endif()

  # The registries are configure inputs: editing a LOKA_TEST line must
  # regenerate CTestTestfile.cmake, not just rebuild the runner binaries.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/tests/ContractTestRegistry.inc
    ${PROJECT_SOURCE_DIR}/tests/ScenarioTestRegistry.inc
    ${PROJECT_SOURCE_DIR}/tests/scenarios/scenarios.txt
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

# Pin the target-local warning contract after every platform/example target
# has been declared. New Loka targets must opt in explicitly.
loka_verify_warning_floor()
