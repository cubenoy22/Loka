cmake_minimum_required(VERSION 3.15)
project(LokaStaticVsDynamic LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

if(NOT WIN32 AND NOT APPLE AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Retro68"))
  add_custom_target(LokaStaticVsDynamic ALL
    COMMAND ${CMAKE_COMMAND} -E echo "LokaStaticVsDynamic has no supported platform target here."
  )
  return()
endif()

if(WIN32)
  set(STATIC_DYNAMIC_SOURCES
    src/main.cpp
  )

  if(NOT TARGET LokaWin32Core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../win32 ${CMAKE_CURRENT_BINARY_DIR}/win32)
  endif()

  option(USE_CONSOLE_SUBSYSTEM "Use console subsystem for debugging" OFF)

  if(USE_CONSOLE_SUBSYSTEM)
    add_executable(LokaStaticVsDynamicWin32 ${STATIC_DYNAMIC_SOURCES})
  else()
    add_executable(LokaStaticVsDynamicWin32 WIN32 ${STATIC_DYNAMIC_SOURCES})
  endif()

  target_include_directories(LokaStaticVsDynamicWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
  )

  target_link_libraries(LokaStaticVsDynamicWin32 PRIVATE LokaWin32Core)
  target_compile_definitions(LokaStaticVsDynamicWin32 PRIVATE
    $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
  )

  if(MSVC)
    target_compile_options(LokaStaticVsDynamicWin32 PRIVATE /utf-8)
  endif()
endif()

if(APPLE)
  set(MACOS_SOURCES
    src/MacOSMain.mm
  )

  add_executable(LokaStaticVsDynamicMacOS ${MACOS_SOURCES})

  target_include_directories(LokaStaticVsDynamicMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
  )

  target_link_libraries(LokaStaticVsDynamicMacOS PRIVATE LokaMacOSCore)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(LokaStaticVsDynamicMacOS PRIVATE "-fno-objc-arc")
  endif()
  target_compile_definitions(LokaStaticVsDynamicMacOS PRIVATE
    $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
  )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
  set(RETRO_SOURCES
    src/main.cpp
  )

  add_application(LokaStaticVsDynamicClassic ${RETRO_SOURCES})

  target_include_directories(LokaStaticVsDynamicClassic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/toolbox/src
  )

  target_link_libraries(LokaStaticVsDynamicClassic PRIVATE LokaAppleToolboxCore)

  target_compile_definitions(LokaStaticVsDynamicClassic PRIVATE
    LOKA_RETRO68=1
    LOKA_PROFILE_FUNC_TICKS=0
  )

  set_target_properties(LokaStaticVsDynamicClassic PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(LokaStaticVsDynamicClassic PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
  )
  target_link_options(LokaStaticVsDynamicClassic PRIVATE
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
      add_custom_command(TARGET LokaStaticVsDynamicClassic POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
          -DSTRIP_TOOL="${RETRO68_STRIP}"
          -DSTRIP_INPUT="$<TARGET_FILE:LokaStaticVsDynamicClassic>.gdb"
          -P "${CMAKE_SOURCE_DIR}/cmake/strip_classic.cmake"
        COMMENT "Stripping Classic debug binary"
      )
    endif()
  endif()
endif()
