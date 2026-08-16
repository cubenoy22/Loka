cmake_minimum_required(VERSION 3.15)
project(LokaTestsToolbox LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME MATCHES "^Retro68$")
  add_custom_target(LokaTestsToolbox ALL
    COMMAND ${CMAKE_COMMAND} -E echo
      "LokaTestsToolbox is Retro68-only. No app is built for this platform."
  )
  return()
endif()

set(LOKA_TESTS_TOOLBOX_TARGET "LokaTestsToolbox${LOKA_CLASSIC_TARGET_SUFFIX}")
set(LOKA_HELLO_WORLD_TESTS_TOOLBOX_TARGET
  "LokaHelloWorldTestsToolbox${LOKA_CLASSIC_TARGET_SUFFIX}"
)
set(LOKA_TUTORIAL_TESTS_TOOLBOX_TARGET
  "LokaTutorialTestsToolbox${LOKA_CLASSIC_TARGET_SUFFIX}"
)
set(LOKA_MINESWEEPER_TESTS_TOOLBOX_TARGET
  "LokaMineSweeperTestsToolbox${LOKA_CLASSIC_TARGET_SUFFIX}"
)
set(LOKA_FLOPPY_BIRD_TESTS_TOOLBOX_TARGET
  "LokaFloppyBirdTestsToolbox${LOKA_CLASSIC_TARGET_SUFFIX}"
)

function(loka_add_toolbox_test_application target core_target)
  add_application(${target} ${ARGN})
  loka_enable_warnings(${target})
  target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/scenarios
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/apple/toolbox/src
  )
  target_link_libraries(${target} PRIVATE ${core_target})
  target_compile_definitions(${target} PRIVATE LOKA_RETRO68=1)
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  target_compile_options(${target} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
    -fno-rtti
  )
  target_link_options(${target} PRIVATE -Wl,--gc-sections)
  if(NOT LOKA_RETRO68_DIAGNOSTICS)
    target_link_options(${target} PRIVATE -Wl,--mac-strip-macsbug)
  endif()
endfunction()

set(LOKA_TESTS_TOOLBOX_SOURCES
  src/main.cpp
  src/ScenarioDriver.cpp
  src/ScenarioDriverSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScrapbookScenarios.cpp
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/src/ScrapbookPackage.cpp
  ${CMAKE_SOURCE_DIR}/common/testing/snap/SnapFormat.cpp
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/Size.r
)

loka_add_toolbox_test_application(
  ${LOKA_TESTS_TOOLBOX_TARGET}
  LokaAppleToolboxCore
  ${LOKA_TESTS_TOOLBOX_SOURCES}
)

set(LOKA_HELLO_WORLD_TESTS_TOOLBOX_SOURCES
  src/HelloWorldScenarioMain.cpp
  src/HelloWorldScenarioDriver.cpp
  src/ScenarioDriverSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/TestingHooks.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/HelloWorldScenarios.cpp
  ${CMAKE_SOURCE_DIR}/example/HelloWorld/src/MainNode.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/HelloWorldSize.r
)
configure_file(
  ${CMAKE_SOURCE_DIR}/example/HelloWorld/Size.r
  ${CMAKE_CURRENT_BINARY_DIR}/HelloWorldSize.r
  COPYONLY
)
loka_add_toolbox_test_application(
  ${LOKA_HELLO_WORLD_TESTS_TOOLBOX_TARGET}
  LokaAppleToolboxTestCore
  ${LOKA_HELLO_WORLD_TESTS_TOOLBOX_SOURCES}
)
target_include_directories(${LOKA_HELLO_WORLD_TESTS_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/example/HelloWorld/src
)
target_compile_definitions(${LOKA_HELLO_WORLD_TESTS_TOOLBOX_TARGET} PRIVATE
  TEST_BUILD
  LOKA_DIAG=1
)

set(LOKA_TUTORIAL_TESTS_TOOLBOX_SOURCES
  src/TutorialScenarioMain.cpp
  src/TutorialScenarioDriver.cpp
  src/ScenarioDriverSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/TestingHooks.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/TutorialScenarios.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/TutorialSize.r
)
configure_file(
  ${CMAKE_SOURCE_DIR}/example/Tutorial/Size.r
  ${CMAKE_CURRENT_BINARY_DIR}/TutorialSize.r
  COPYONLY
)
loka_add_toolbox_test_application(
  ${LOKA_TUTORIAL_TESTS_TOOLBOX_TARGET}
  LokaAppleToolboxTestCore
  ${LOKA_TUTORIAL_TESTS_TOOLBOX_SOURCES}
)
target_include_directories(${LOKA_TUTORIAL_TESTS_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/example/Tutorial/src
)
target_compile_definitions(${LOKA_TUTORIAL_TESTS_TOOLBOX_TARGET} PRIVATE
  TEST_BUILD
  LOKA_DIAG=1
)

set(LOKA_MINESWEEPER_TESTS_TOOLBOX_SOURCES
  src/MineSweeperScenarioMain.cpp
  src/MineSweeperScenarioDriver.cpp
  src/ScenarioDriverSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/TestingHooks.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/MineSweeperScenarios.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/MineSweeperSize.r
)
configure_file(
  ${CMAKE_SOURCE_DIR}/example/MineSweeper/Size.r
  ${CMAKE_CURRENT_BINARY_DIR}/MineSweeperSize.r
  COPYONLY
)
loka_add_toolbox_test_application(
  ${LOKA_MINESWEEPER_TESTS_TOOLBOX_TARGET}
  LokaAppleToolboxTestCore
  ${LOKA_MINESWEEPER_TESTS_TOOLBOX_SOURCES}
)
target_include_directories(${LOKA_MINESWEEPER_TESTS_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/example/MineSweeper/src
)
target_compile_definitions(${LOKA_MINESWEEPER_TESTS_TOOLBOX_TARGET} PRIVATE
  TEST_BUILD
  LOKA_DIAG=1
)

set(LOKA_FLOPPY_BIRD_TESTS_TOOLBOX_SOURCES
  src/FloppyBirdScenarioMain.cpp
  src/FloppyBirdScenarioDriver.cpp
  src/ScenarioDriverSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/TestingHooks.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/FloppyBirdScenarios.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/FloppyBirdSize.r
)
configure_file(
  ${CMAKE_SOURCE_DIR}/example/FloppyBird/Size.r
  ${CMAKE_CURRENT_BINARY_DIR}/FloppyBirdSize.r
  COPYONLY
)
loka_add_toolbox_test_application(
  ${LOKA_FLOPPY_BIRD_TESTS_TOOLBOX_TARGET}
  LokaAppleToolboxTestCore
  ${LOKA_FLOPPY_BIRD_TESTS_TOOLBOX_SOURCES}
)
target_include_directories(${LOKA_FLOPPY_BIRD_TESTS_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/example/FloppyBird/src
)
target_compile_definitions(${LOKA_FLOPPY_BIRD_TESTS_TOOLBOX_TARGET} PRIVATE
  TEST_BUILD
  LOKA_DIAG=1
)

set(LOKA_STANDALONE_FLOW_TOOLBOX_TARGET
  # Keep the generated application name within Classic HFS's 31-character limit.
  "LokaScrapbookStandaloneFlow${LOKA_CLASSIC_TARGET_SUFFIX}"
)

set(LOKA_HELLO_WORLD_STANDALONE_FLOW_TOOLBOX_TARGET
  # Keep the generated application name within Classic HFS's 31-character limit.
  "LokaHelloStandaloneFlow${LOKA_CLASSIC_TARGET_SUFFIX}"
)
set(LOKA_TUTORIAL_STANDALONE_FLOW_TOOLBOX_TARGET
  # Keep the generated application name within Classic HFS's 31-character limit.
  "LokaTutorialStandaloneFlow${LOKA_CLASSIC_TARGET_SUFFIX}"
)
set(LOKA_MINESWEEPER_STANDALONE_FLOW_TOOLBOX_TARGET
  # Keep the generated application name within Classic HFS's 31-character limit.
  "LokaMineStandaloneFlow${LOKA_CLASSIC_TARGET_SUFFIX}"
)
set(LOKA_FLOPPY_BIRD_STANDALONE_FLOW_TOOLBOX_TARGET
  # Keep the generated application name within Classic HFS's 31-character limit.
  "LokaFloppyStandaloneFlow${LOKA_CLASSIC_TARGET_SUFFIX}"
)
set(LOKA_HELLO_WORLD_STANDALONE_FLOW_TOOLBOX_SOURCES
  src/HelloWorldStandaloneFlowMain.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/HelloWorldStandaloneFlowApplication.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/HelloWorldStandaloneFlowAppConfig.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/StandaloneScenarioSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/TestingHooks.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/HelloWorldScenarios.cpp
  ${CMAKE_SOURCE_DIR}/example/HelloWorld/src/MainNode.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/HelloWorldStandaloneSize.r
)
set(LOKA_TUTORIAL_STANDALONE_FLOW_TOOLBOX_SOURCES
  src/TutorialStandaloneFlowMain.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/TutorialStandaloneFlowApplication.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/TutorialStandaloneFlowAppConfig.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/StandaloneScenarioSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/TestingHooks.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/TutorialScenarios.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/TutorialStandaloneSize.r
)
set(LOKA_MINESWEEPER_STANDALONE_FLOW_TOOLBOX_SOURCES
  src/MineSweeperStandaloneFlowMain.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/MineSweeperStandaloneFlowApplication.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/MineSweeperStandaloneFlowAppConfig.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/StandaloneScenarioSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/TestingHooks.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/MineSweeperScenarios.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/MineSweeperStandaloneSize.r
)
set(LOKA_FLOPPY_BIRD_STANDALONE_FLOW_TOOLBOX_SOURCES
  src/FloppyBirdStandaloneFlowMain.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/FloppyBirdStandaloneFlowApplication.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/FloppyBirdStandaloneFlowAppConfig.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/StandaloneScenarioSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/TestingHooks.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/FloppyBirdScenarios.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/FloppyBirdStandaloneSize.r
)
set(LOKA_STANDALONE_FLOW_TOOLBOX_SOURCES
  src/StandaloneFlowMain.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/StandaloneFlowApplication.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/ScrapbookStandaloneFlowAppConfig.cpp
  ${CMAKE_SOURCE_DIR}/tests/standalone/StandaloneScenarioSupport.cpp
  ${CMAKE_SOURCE_DIR}/tests/TestingHooks.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScenarioTypes.cpp
  ${CMAKE_SOURCE_DIR}/tests/scenarios/ScrapbookScenarios.cpp
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/src/ScrapbookPackage.cpp
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/Size.r
)

loka_add_toolbox_test_application(
  ${LOKA_STANDALONE_FLOW_TOOLBOX_TARGET}
  LokaAppleToolboxTestCore
  ${LOKA_STANDALONE_FLOW_TOOLBOX_SOURCES}
)
set_property(TARGET ${LOKA_STANDALONE_FLOW_TOOLBOX_TARGET}
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
set_property(TARGET ${LOKA_STANDALONE_FLOW_TOOLBOX_TARGET}_APPL
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
target_include_directories(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/src
)

target_include_directories(${LOKA_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/standalone
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/src
)

target_compile_definitions(${LOKA_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  TEST_BUILD
  LOKA_DIAG=1
)

configure_file(
  ${CMAKE_SOURCE_DIR}/example/HelloWorld/Size.r
  ${CMAKE_CURRENT_BINARY_DIR}/HelloWorldStandaloneSize.r
  COPYONLY
)
loka_add_toolbox_test_application(
  ${LOKA_HELLO_WORLD_STANDALONE_FLOW_TOOLBOX_TARGET}
  LokaAppleToolboxTestCore
  ${LOKA_HELLO_WORLD_STANDALONE_FLOW_TOOLBOX_SOURCES}
)
set_property(TARGET ${LOKA_HELLO_WORLD_STANDALONE_FLOW_TOOLBOX_TARGET}
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
set_property(TARGET ${LOKA_HELLO_WORLD_STANDALONE_FLOW_TOOLBOX_TARGET}_APPL
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
target_include_directories(${LOKA_HELLO_WORLD_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/standalone
  ${CMAKE_SOURCE_DIR}/example/HelloWorld/src
)
target_compile_definitions(${LOKA_HELLO_WORLD_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  TEST_BUILD
  LOKA_DIAG=1
)

configure_file(
  ${CMAKE_SOURCE_DIR}/example/Tutorial/Size.r
  ${CMAKE_CURRENT_BINARY_DIR}/TutorialStandaloneSize.r
  COPYONLY
)
loka_add_toolbox_test_application(
  ${LOKA_TUTORIAL_STANDALONE_FLOW_TOOLBOX_TARGET}
  LokaAppleToolboxTestCore
  ${LOKA_TUTORIAL_STANDALONE_FLOW_TOOLBOX_SOURCES}
)
set_property(TARGET ${LOKA_TUTORIAL_STANDALONE_FLOW_TOOLBOX_TARGET}
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
set_property(TARGET ${LOKA_TUTORIAL_STANDALONE_FLOW_TOOLBOX_TARGET}_APPL
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
target_include_directories(${LOKA_TUTORIAL_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/standalone
  ${CMAKE_SOURCE_DIR}/example/Tutorial/src
)
target_compile_definitions(${LOKA_TUTORIAL_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  TEST_BUILD
  LOKA_DIAG=1
)

configure_file(
  ${CMAKE_SOURCE_DIR}/example/MineSweeper/Size.r
  ${CMAKE_CURRENT_BINARY_DIR}/MineSweeperStandaloneSize.r
  COPYONLY
)
loka_add_toolbox_test_application(
  ${LOKA_MINESWEEPER_STANDALONE_FLOW_TOOLBOX_TARGET}
  LokaAppleToolboxTestCore
  ${LOKA_MINESWEEPER_STANDALONE_FLOW_TOOLBOX_SOURCES}
)
set_property(TARGET ${LOKA_MINESWEEPER_STANDALONE_FLOW_TOOLBOX_TARGET}
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
set_property(TARGET ${LOKA_MINESWEEPER_STANDALONE_FLOW_TOOLBOX_TARGET}_APPL
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
target_include_directories(${LOKA_MINESWEEPER_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/standalone
  ${CMAKE_SOURCE_DIR}/example/MineSweeper/src
)
target_compile_definitions(${LOKA_MINESWEEPER_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  TEST_BUILD
  LOKA_DIAG=1
)

configure_file(
  ${CMAKE_SOURCE_DIR}/example/FloppyBird/Size.r
  ${CMAKE_CURRENT_BINARY_DIR}/FloppyBirdStandaloneSize.r
  COPYONLY
)
loka_add_toolbox_test_application(
  ${LOKA_FLOPPY_BIRD_STANDALONE_FLOW_TOOLBOX_TARGET}
  LokaAppleToolboxTestCore
  ${LOKA_FLOPPY_BIRD_STANDALONE_FLOW_TOOLBOX_SOURCES}
)
set_property(TARGET ${LOKA_FLOPPY_BIRD_STANDALONE_FLOW_TOOLBOX_TARGET}
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
set_property(TARGET ${LOKA_FLOPPY_BIRD_STANDALONE_FLOW_TOOLBOX_TARGET}_APPL
  PROPERTY EXCLUDE_FROM_ALL TRUE
)
target_include_directories(${LOKA_FLOPPY_BIRD_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/tests/standalone
  ${CMAKE_SOURCE_DIR}/example/FloppyBird/src
)
target_compile_definitions(${LOKA_FLOPPY_BIRD_STANDALONE_FLOW_TOOLBOX_TARGET} PRIVATE
  TEST_BUILD
  LOKA_DIAG=1
)

if(NOT LOKA_SCRAPBOOK_PAGE_COUNT STREQUAL "")
  target_compile_definitions(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
    LOKA_SCRAPBOOK_PAGE_COUNT=${LOKA_SCRAPBOOK_PAGE_COUNT}
  )
endif()

if(NOT LOKA_SCRAPBOOK_ID_SPACE_STAMP STREQUAL "")
  target_compile_definitions(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
    LOKA_SCRAPBOOK_ID_SPACE_STAMP=${LOKA_SCRAPBOOK_ID_SPACE_STAMP}
  )
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/ASSETS.LRP
  ASSETS.LRP
  COPYONLY
)
