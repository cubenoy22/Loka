cmake_minimum_required(VERSION 3.15)
project(Loka LANGUAGES CXX)

option(LOKA_ENABLE_APPLE "Enable Apple platform targets" ON)
option(TEST_BUILD "Enable test build mode" OFF)

if(TEST_BUILD)
  add_definitions(-DTEST_BUILD)
endif()

if(WIN32)
  add_subdirectory(win32)
endif()

if(APPLE AND LOKA_ENABLE_APPLE)
  add_subdirectory(apple)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
  add_subdirectory(apple/common/carbon)
  add_subdirectory(apple/toolbox)
endif()

if(WIN32 OR (APPLE AND LOKA_ENABLE_APPLE) OR (CMAKE_SYSTEM_NAME STREQUAL "Retro68"))
add_subdirectory(example/HelloWorld)
add_subdirectory(example/MineSweeper)
add_subdirectory(example/SimpleViewer)
endif()

# Linux/WSL headless tests (Flow DSL focused).
if(NOT WIN32 AND NOT APPLE AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Retro68"))
  file(GLOB_RECURSE LOKA_COMMON_SOURCES
    ${PROJECT_SOURCE_DIR}/common/*.cpp
  )

  set(LOKA_FLOW_TEST_SOURCES
    ${PROJECT_SOURCE_DIR}/tests/StateNotifyTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/FlowDslTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/AttrDslTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/FlowDslTestMain.cpp
  )

  add_executable(LokaFlowDslTests
    ${LOKA_FLOW_TEST_SOURCES}
    ${LOKA_COMMON_SOURCES}
  )
  target_include_directories(LokaFlowDslTests PRIVATE
    ${PROJECT_SOURCE_DIR}/common
    ${PROJECT_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/example/SimpleViewer/src
  )
  set_target_properties(LokaFlowDslTests PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  enable_testing()
  add_test(NAME LokaFlowDslTests COMMAND LokaFlowDslTests)
endif()
