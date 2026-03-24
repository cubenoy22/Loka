cmake_minimum_required(VERSION 3.15)
project(LokaSimpleViewer LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

if(NOT WIN32 AND NOT APPLE AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Retro68"))
  add_custom_target(LokaSimpleViewer ALL
    COMMAND ${CMAKE_COMMAND} -E echo "LokaSimpleViewer has no supported platform target here."
  )
  return()
endif()

if(WIN32)
  set(SIMPLEVIEWER_SOURCES
    src/main.cpp
  )

  if(NOT TARGET LokaWin32Core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../win32 ${CMAKE_CURRENT_BINARY_DIR}/win32)
  endif()

  option(USE_CONSOLE_SUBSYSTEM "Use console subsystem for debugging" OFF)

  if(USE_CONSOLE_SUBSYSTEM)
    add_executable(LokaSimpleViewerWin32 ${SIMPLEVIEWER_SOURCES})
  else()
    add_executable(LokaSimpleViewerWin32 WIN32 ${SIMPLEVIEWER_SOURCES})
  endif()

  target_include_directories(LokaSimpleViewerWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
  )

  target_link_libraries(LokaSimpleViewerWin32 PRIVATE LokaWin32Core)
  target_compile_definitions(LokaSimpleViewerWin32 PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${LOKA_ENABLE_DEBUG_LOGS}>>:LOKA_DEBUG_RECOMPOSE=1>
  )

  if(MSVC)
    target_compile_options(LokaSimpleViewerWin32 PRIVATE /utf-8)
  endif()
endif()

if(APPLE)
  set(MACOS_SOURCES
    src/MacOSMain.mm
  )

  add_executable(LokaSimpleViewerMacOS ${MACOS_SOURCES})

  target_include_directories(LokaSimpleViewerMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
  )

  target_link_libraries(LokaSimpleViewerMacOS PRIVATE LokaMacOSCore)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(LokaSimpleViewerMacOS PRIVATE "-fno-objc-arc")
  endif()
  target_compile_definitions(LokaSimpleViewerMacOS PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${LOKA_ENABLE_DEBUG_LOGS}>>:LOKA_DEBUG_RECOMPOSE=1>
  )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
  set(RETRO_SOURCES
    src/main.cpp
  )

  add_application(LokaSimpleViewerClassic ${RETRO_SOURCES})

  target_include_directories(LokaSimpleViewerClassic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/toolbox/src
  )

  target_link_libraries(LokaSimpleViewerClassic PRIVATE LokaAppleToolboxCore)

  target_compile_definitions(LokaSimpleViewerClassic PRIVATE LOKA_RETRO68=1 LOKA_PROFILE_FUNC_TICKS=1)

  set_target_properties(LokaSimpleViewerClassic PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(LokaSimpleViewerClassic PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
  )
  target_link_options(LokaSimpleViewerClassic PRIVATE
    -Wl,--gc-sections
  )

  option(LOKA_CLASSIC_STRIP "Strip Classic Mac binaries after link" ON)
  if(LOKA_CLASSIC_STRIP)
    find_program(RETRO68_STRIP
      NAMES m68k-apple-macos-strip powerpc-apple-macos-strip
      HINTS
        "$ENV{RETRO68_BUILD_DIR}/toolchain/bin"
        "${RETRO68_BUILD_DIR}/toolchain/bin"
        "${RETRO68_TOOLCHAIN_DIR}/../bin"
    )
    if(RETRO68_STRIP)
      add_custom_command(TARGET LokaSimpleViewerClassic POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
          -DSTRIP_TOOL="${RETRO68_STRIP}"
          -DSTRIP_INPUT="$<TARGET_FILE:LokaSimpleViewerClassic>.gdb"
          -P "${CMAKE_SOURCE_DIR}/cmake/strip_classic.cmake"
        COMMENT "Stripping Classic debug binary"
      )
    endif()
  endif()
endif()
