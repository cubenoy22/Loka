cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 98)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

option(TEST_BUILD "Include unit test sources in app target" OFF)
option(LOKA_MAC_OS_10_4 "Enable macOS 10.4u SDK mode" OFF)
option(LOKA_BUILD_MACOS_TESTS "Build macOS unit test target (LokaTestsMacOS)" ON)
set(LOKA_MAC_OS_10_4_SYSROOT "/Developer/SDKs/MacOSX10.4u.sdk" CACHE PATH
  "Path to macOS 10.4u SDK used when LOKA_MAC_OS_10_4=ON")

if(LOKA_MAC_OS_10_4)
  if(NOT EXISTS "${LOKA_MAC_OS_10_4_SYSROOT}")
    message(FATAL_ERROR "LOKA_MAC_OS_10_4_SYSROOT not found: ${LOKA_MAC_OS_10_4_SYSROOT}")
  endif()
  set(CMAKE_OSX_SYSROOT "${LOKA_MAC_OS_10_4_SYSROOT}" CACHE PATH "macOS SDK path" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.4" CACHE STRING "Minimum macOS deployment target" FORCE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options("-stdlib=libstdc++")
    add_link_options("-stdlib=libstdc++")
  endif()
endif()

add_library(LokaAppleMacOSAppKit INTERFACE)

target_link_libraries(LokaAppleMacOSAppKit INTERFACE
  LokaAppleCommonCocoa
  "-framework AppKit"
  "-framework Foundation"
  "-framework CoreFoundation"
)

target_include_directories(LokaAppleMacOSAppKit INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

file(GLOB_RECURSE COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.cpp)

set(LOKA_MACOS_CORE_SOURCES
  src/MacApp.mm
  src/DebugLog.mm
  src/MacPlatformContext.mm
  src/MacScenePlatformController.mm
  src/MacWindow.mm
  src/context/MacButtonContext.mm
  src/context/MacCellContext.mm
  src/context/MacEditTextContext.mm
  src/context/MacOpenFileDialogContext.mm
  src/context/MacTextContext.mm
  src/context/MacPopupMenuContext.mm
  src/context/MacImageViewContext.mm
  ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/AppleString.mm
)

add_library(LokaMacOSCore STATIC ${LOKA_MACOS_CORE_SOURCES} ${COMMON_SOURCES})

target_include_directories(LokaMacOSCore PUBLIC
  ${PROJECT_SOURCE_DIR}/common
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/src/context
)

target_link_libraries(LokaMacOSCore PUBLIC LokaAppleMacOSAppKit)

# Loka core is ObjC1/non-ARC by design; do not force ARC flags here.
target_compile_definitions(LokaMacOSCore PRIVATE
  $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
)

set(LOKA_ENABLE_MACOS_TESTS ${LOKA_BUILD_MACOS_TESTS})
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.6")
    set(LOKA_ENABLE_MACOS_TESTS OFF)
    message(STATUS "Disabling LokaTestsMacOS for GNU C++ < 4.6 due to template limitations in tests.")
  endif()
endif()

set(LOKA_MACOS_TEST_SOURCES
  ${PROJECT_SOURCE_DIR}/tests/StateNotifyTests.cpp
  ${PROJECT_SOURCE_DIR}/tests/Tests.cpp
  ${PROJECT_SOURCE_DIR}/tests/FlowDslTests.cpp
  ${PROJECT_SOURCE_DIR}/tests/AttrDslTests.cpp
  ${PROJECT_SOURCE_DIR}/tests/TestMain.cpp
  ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/AppleString.mm
)

if(NOT LOKA_MAC_OS_10_4 AND LOKA_ENABLE_MACOS_TESTS)
  add_executable(LokaTestsMacOS ${LOKA_MACOS_TEST_SOURCES} ${COMMON_SOURCES})

  target_include_directories(LokaTestsMacOS PRIVATE
    ${PROJECT_SOURCE_DIR}/common
    ${PROJECT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_LIST_DIR}/src
  )

  target_link_libraries(LokaTestsMacOS PRIVATE LokaAppleCommonCocoa "-framework CoreFoundation")
  target_compile_definitions(LokaTestsMacOS PRIVATE
    $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
  )
endif()
