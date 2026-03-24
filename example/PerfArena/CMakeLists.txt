cmake_minimum_required(VERSION 3.15)
project(LokaPerfArena LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

if(NOT WIN32 AND NOT APPLE)
  add_custom_target(LokaPerfArena ALL
    COMMAND ${CMAKE_COMMAND} -E echo "LokaPerfArena has no supported platform target here."
  )
  return()
endif()

if(WIN32)
  set(PERF_ARENA_SOURCES
    src/main.cpp
  )

  if(NOT TARGET LokaWin32Core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../win32 ${CMAKE_CURRENT_BINARY_DIR}/win32)
  endif()

  option(USE_CONSOLE_SUBSYSTEM "Use console subsystem for debugging" OFF)

  if(USE_CONSOLE_SUBSYSTEM)
    add_executable(LokaPerfArenaWin32 ${PERF_ARENA_SOURCES})
  else()
    add_executable(LokaPerfArenaWin32 WIN32 ${PERF_ARENA_SOURCES})
  endif()

  target_include_directories(LokaPerfArenaWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
  )

  target_link_libraries(LokaPerfArenaWin32 PRIVATE LokaWin32Core)
  target_compile_definitions(LokaPerfArenaWin32 PRIVATE
    LOKA_PROFILE_FUNC_TICKS=1
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${LOKA_ENABLE_DEBUG_LOGS}>>:LOKA_DEBUG_RECOMPOSE=1>
  )

  if(MSVC)
    target_compile_options(LokaPerfArenaWin32 PRIVATE /utf-8)
  endif()
endif()

if(APPLE)
  set(MACOS_SOURCES
    src/MacOSMain.mm
  )

  add_executable(LokaPerfArenaMacOS ${MACOS_SOURCES})

  target_include_directories(LokaPerfArenaMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
  )

  target_link_libraries(LokaPerfArenaMacOS PRIVATE LokaMacOSCore)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(LokaPerfArenaMacOS PRIVATE "-fno-objc-arc")
  endif()
  target_compile_definitions(LokaPerfArenaMacOS PRIVATE
    LOKA_PROFILE_FUNC_TICKS=1
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${LOKA_ENABLE_DEBUG_LOGS}>>:LOKA_DEBUG_RECOMPOSE=1>
  )
endif()
