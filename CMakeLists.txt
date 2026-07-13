cmake_minimum_required(VERSION 3.15)
project(Loka LANGUAGES CXX)

option(LOKA_ENABLE_APPLE "Enable Apple platform targets" ON)
option(LOKA_MACOS_EXPLICIT_NO_ARC_FLAGS "Add explicit -fno-objc-arc flags to macOS example targets" ON)
option(LOKA_MACOS_SUPPRESS_ARCLITE_LINK "Remove libarclite from generated macOS link settings" OFF)
option(TEST_BUILD "Enable test build mode" OFF)
option(LOKA_LIFECYCLE_AUDIT "Enable lifecycle alive-counter auditing" OFF)

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
# suite below.
enable_testing()

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

# Linux/WSL headless tests (Flow DSL focused).
if(NOT WIN32 AND NOT APPLE AND NOT LOKA_CLASSIC_MAC)
  file(GLOB_RECURSE LOKA_COMMON_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/common/*.cpp
  )

  add_executable(LokaFlowDslTests
    ${LOKA_SHARED_TEST_SOURCES}
    ${LOKA_COMMON_SOURCES}
  )
  target_include_directories(LokaFlowDslTests PRIVATE
    ${PROJECT_SOURCE_DIR}/common
    ${PROJECT_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/example/SimpleViewer/src
    ${PROJECT_SOURCE_DIR}/example/HelloWorld/src
  )
  set_target_properties(LokaFlowDslTests PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  target_compile_definitions(LokaFlowDslTests PRIVATE TEST_BUILD)

  enable_testing()
  add_test(NAME LokaFlowDslTests COMMAND LokaFlowDslTests)
endif()
