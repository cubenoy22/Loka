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

set(DECLARA_MACOS_SOURCES
  src/main.mm
  ${PROJECT_SOURCE_DIR}/apple/common/cocoa/src/AppleString.mm
)

if(TEST_BUILD)
  list(APPEND DECLARA_MACOS_SOURCES
    ${PROJECT_SOURCE_DIR}/tests/Tests.cpp
  )
endif()

file(GLOB_RECURSE COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.cpp)

add_executable(DeclaraHelloMacOS ${DECLARA_MACOS_SOURCES} ${COMMON_SOURCES})

target_include_directories(DeclaraHelloMacOS PRIVATE
  ${PROJECT_SOURCE_DIR}/common
  ${PROJECT_SOURCE_DIR}/tests
  ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(DeclaraHelloMacOS PRIVATE DeclaraAppleMacOSAppKit)

target_compile_options(DeclaraHelloMacOS PRIVATE "-fobjc-arc")

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
