cmake_minimum_required(VERSION 3.15)
project(LokaLazyList LANGUAGES CXX)

set(LAZYLIST_CAPACITY 100 CACHE STRING "LazyList card capacity (1..1000)")
set(LOKA_LAZYFLEX_MAX_ITEMS 256 CACHE STRING "LazyFlex example visibility capacity")
add_compile_definitions(LAZYLIST_CAPACITY=${LAZYLIST_CAPACITY}
  LOKA_LAZYFLEX_MAX_ITEMS=${LOKA_LAZYFLEX_MAX_ITEMS})

if(APPLE)
  enable_language(OBJCXX)
  loka_suppress_arclite_link()
endif()

if(NOT WIN32 AND NOT APPLE)
  if(NOT (CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$"))
    add_custom_target(LokaLazyList ALL
      COMMAND ${CMAKE_COMMAND} -E echo "LokaLazyList has no supported platform target here."
    )
    return()
  endif()
endif()

if(WIN32)
  include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/LokaWin32VisualStyles.cmake)
  add_executable(LokaLazyListWin32 WIN32
    src/main.cpp
  )
  loka_enable_warnings(LokaLazyListWin32)
  loka_enable_win32_visual_styles(LokaLazyListWin32)

  set_target_properties(LokaLazyListWin32 PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_include_directories(LokaLazyListWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(LokaLazyListWin32 PRIVATE LokaWin32Core)

  if(MSVC)
    target_compile_options(LokaLazyListWin32 PRIVATE /utf-8)
  endif()
endif()

if(APPLE)
  file(GLOB_RECURSE MACOS_HEADERS src/*.hpp)

  add_executable(LokaLazyListMacOS
    src/MacOSMain.mm
    ${MACOS_HEADERS}
  )
  loka_enable_warnings(LokaLazyListMacOS)

  set_target_properties(LokaLazyListMacOS PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_include_directories(LokaLazyListMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(LokaLazyListMacOS PRIVATE LokaMacOSCore)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND LOKA_MACOS_EXPLICIT_NO_ARC_FLAGS)
    target_compile_options(LokaLazyListMacOS PRIVATE "-fno-objc-arc")
  endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  set(LOKA_LAZYLIST_CLASSIC_TARGET "LokaLazyList${LOKA_CLASSIC_TARGET_SUFFIX}")

  add_application(${LOKA_LAZYLIST_CLASSIC_TARGET}
    src/main.cpp
    Size.r
  )
  loka_enable_warnings(${LOKA_LAZYLIST_CLASSIC_TARGET})

  target_include_directories(${LOKA_LAZYLIST_CLASSIC_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(${LOKA_LAZYLIST_CLASSIC_TARGET} PRIVATE LokaAppleToolboxCore)

  target_compile_definitions(${LOKA_LAZYLIST_CLASSIC_TARGET} PRIVATE LOKA_RETRO68=1)

  set_target_properties(${LOKA_LAZYLIST_CLASSIC_TARGET} PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(${LOKA_LAZYLIST_CLASSIC_TARGET} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
    -fno-rtti
  )
  target_link_options(${LOKA_LAZYLIST_CLASSIC_TARGET} PRIVATE
    -Wl,--gc-sections
  )

  if(CMAKE_SYSTEM_NAME STREQUAL "Retro68" AND NOT LOKA_RETRO68_DIAGNOSTICS)
    target_link_options(${LOKA_LAZYLIST_CLASSIC_TARGET} PRIVATE -Wl,--mac-strip-macsbug)
  endif()
endif()
