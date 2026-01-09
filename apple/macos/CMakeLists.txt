cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 98)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

option(TEST_BUILD "Include unit test sources in app target" OFF)

add_library(DeclaraAppleMacOSAppKit INTERFACE)

target_link_libraries(DeclaraAppleMacOSAppKit INTERFACE
  DeclaraAppleCommonCocoa
  "-framework AppKit"
  "-framework Foundation"
  "-framework CoreFoundation"
)

target_include_directories(DeclaraAppleMacOSAppKit INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

file(GLOB_RECURSE COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.cpp)

set(DECLARA_MACOS_CORE_SOURCES
  src/MacApp.mm
  src/DebugLog.mm
  src/MacPlatformContext.mm
  src/MacScenePlatformController.mm
  src/MacWindow.mm
  src/context/MacButtonContext.mm
  src/context/MacEditTextContext.mm
  src/context/MacTextContext.mm
  src/context/MacPopupMenuContext.mm
  ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/AppleString.mm
)

add_library(DeclaraMacOSCore STATIC ${DECLARA_MACOS_CORE_SOURCES} ${COMMON_SOURCES})

target_include_directories(DeclaraMacOSCore PUBLIC
  ${PROJECT_SOURCE_DIR}/common
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/src/context
)

target_link_libraries(DeclaraMacOSCore PUBLIC DeclaraAppleMacOSAppKit)

target_compile_options(DeclaraMacOSCore PRIVATE "-fobjc-arc")
target_compile_definitions(DeclaraMacOSCore PRIVATE
  $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
)

set(DECLARA_MACOS_TEST_SOURCES
  ${PROJECT_SOURCE_DIR}/tests/Tests.cpp
  ${PROJECT_SOURCE_DIR}/tests/TestMain.cpp
  ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/AppleString.mm
)

add_executable(DeclaraTestsMacOS ${DECLARA_MACOS_TEST_SOURCES} ${COMMON_SOURCES})

target_include_directories(DeclaraTestsMacOS PRIVATE
  ${PROJECT_SOURCE_DIR}/common
  ${PROJECT_SOURCE_DIR}/tests
  ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(DeclaraTestsMacOS PRIVATE DeclaraAppleCommonCocoa "-framework CoreFoundation")
target_compile_definitions(DeclaraTestsMacOS PRIVATE
  $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
)
