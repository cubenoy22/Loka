cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 98)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

option(TEST_BUILD "Include unit test sources in app target" OFF)

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
  ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/AppleString.mm
)

add_library(LokaMacOSCore STATIC ${LOKA_MACOS_CORE_SOURCES} ${COMMON_SOURCES})

target_include_directories(LokaMacOSCore PUBLIC
  ${PROJECT_SOURCE_DIR}/common
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/src/context
)

target_link_libraries(LokaMacOSCore PUBLIC LokaAppleMacOSAppKit)

target_compile_options(LokaMacOSCore PRIVATE "-fobjc-arc")
target_compile_definitions(LokaMacOSCore PRIVATE
  $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
)

set(LOKA_MACOS_TEST_SOURCES
  ${PROJECT_SOURCE_DIR}/tests/Tests.cpp
  ${PROJECT_SOURCE_DIR}/tests/TestMain.cpp
  ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/AppleString.mm
)

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
