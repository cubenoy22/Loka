cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 98)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

option(TEST_BUILD "Include unit test sources in app target" OFF)
option(LOKA_MAC_OS_10_4 "Enable macOS 10.4u SDK mode" OFF)
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

file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/*.cpp)
file(GLOB_RECURSE COMMON_HEADERS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/*.hpp)
file(GLOB_RECURSE LOKA_MACOS_CORE_HEADERS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/src/*.hpp
  ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/*.hpp
)

set(LOKA_MACOS_CORE_SOURCES
  src/MacBootstrap.mm
  src/MacApp.mm
  src/MacBuiltInSupport.mm
  src/MacPlatformContext.mm
  src/MacProfiler.cpp
  src/MacScenePlatformController.mm
  src/MacWindow.mm
  src/context/MacButtonContext.mm
  src/context/MacCellContext.mm
  src/context/MacEditTextContext.mm
  src/context/MacOpenFileDialogContext.mm
  src/context/MacTextContext.mm
  src/context/MacPopupMenuContext.mm
  src/context/MacImageViewContext.mm
  src/context/MacRectSurfaceContext.mm
  src/context/MacNodeContextMapper.mm
  ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/AppleString.mm
)

add_library(LokaMacOSCore STATIC
  ${LOKA_MACOS_CORE_SOURCES}
  ${COMMON_SOURCES}
  ${COMMON_HEADERS}
  ${LOKA_MACOS_CORE_HEADERS}
)

target_include_directories(LokaMacOSCore PUBLIC
  ${PROJECT_SOURCE_DIR}/common
)

target_include_directories(LokaMacOSCore PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/src/context
)

target_link_libraries(LokaMacOSCore PUBLIC LokaAppleMacOSAppKit)

# Loka core is ObjC1/non-ARC by design; do not force ARC flags here.

file(GLOB_RECURSE LOKA_MACOS_TEST_HEADERS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/*.hpp)

if(NOT LOKA_MAC_OS_10_4)
  # The test binary compiles the common sources itself so TEST_BUILD applies
  # to them uniformly (mirrors the Linux LokaFlowDslTests target). Linking the
  # production LokaMacOSCore instead would mix TEST_BUILD and non-TEST_BUILD
  # compilations of the same classes (ODR/layout divergence).
  file(GLOB_RECURSE LOKA_MACOS_TEST_COMMON_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/common/*.cpp
  )
  # EXCLUDE_FROM_ALL: this target recompiles all common sources under
  # TEST_BUILD, so keep it out of Build All (and generated Xcode projects);
  # the macos-tests build preset builds it explicitly.
  add_executable(LokaTestsMacOS EXCLUDE_FROM_ALL
    ${LOKA_SHARED_TEST_SOURCES}
    ${LOKA_MACOS_TEST_COMMON_SOURCES}
    ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/AppleString.mm
    ${LOKA_MACOS_TEST_HEADERS}
  )

  target_include_directories(LokaTestsMacOS PRIVATE
    ${PROJECT_SOURCE_DIR}/common
    ${PROJECT_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/example/HelloWorld/src
    ${CMAKE_CURRENT_LIST_DIR}/src
  )

  target_compile_definitions(LokaTestsMacOS PRIVATE TEST_BUILD)
  target_link_libraries(LokaTestsMacOS PRIVATE
    "-framework Foundation"
    "-framework CoreFoundation"
  )

  enable_testing()
  add_test(NAME LokaTestsMacOS COMMAND LokaTestsMacOS)
endif()
