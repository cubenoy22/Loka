cmake_minimum_required(VERSION 3.15)
project(LokaTutorial LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

if(NOT WIN32 AND NOT APPLE AND NOT (CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$"))
  add_custom_target(LokaTutorial ALL
    COMMAND ${CMAKE_COMMAND} -E echo "LokaTutorial has no supported platform target here."
  )
  return()
endif()

if(WIN32)
  set(TUTORIAL_SOURCES
    src/main.cpp
  )

  if(NOT TARGET LokaWin32Core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../win32 ${CMAKE_CURRENT_BINARY_DIR}/win32)
  endif()

  option(USE_CONSOLE_SUBSYSTEM "Use console subsystem for debugging" OFF)

  if(USE_CONSOLE_SUBSYSTEM)
    add_executable(LokaTutorialWin32 ${TUTORIAL_SOURCES})
  else()
    add_executable(LokaTutorialWin32 WIN32 ${TUTORIAL_SOURCES})
  endif()

  target_include_directories(LokaTutorialWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(LokaTutorialWin32 PRIVATE LokaWin32Core)

  if(MSVC)
    target_compile_options(LokaTutorialWin32 PRIVATE /utf-8)
  endif()
endif()

if(APPLE)
  set(MACOS_SOURCES
    src/MacOSMain.mm
  )
  file(GLOB_RECURSE MACOS_HEADERS src/*.hpp)

  add_executable(LokaTutorialMacOS ${MACOS_SOURCES} ${MACOS_HEADERS})

  target_include_directories(LokaTutorialMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
  )

  target_link_libraries(LokaTutorialMacOS PRIVATE LokaMacOSCore)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND LOKA_MACOS_EXPLICIT_NO_ARC_FLAGS)
    target_compile_options(LokaTutorialMacOS PRIVATE "-fno-objc-arc")
  endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  set(LOKA_TUTORIAL_CLASSIC_TARGET "LokaTutorial${LOKA_CLASSIC_TARGET_SUFFIX}")
  set(RETRO_SOURCES
    src/main.cpp
  )

  add_application(${LOKA_TUTORIAL_CLASSIC_TARGET} ${RETRO_SOURCES})

  target_include_directories(${LOKA_TUTORIAL_CLASSIC_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(${LOKA_TUTORIAL_CLASSIC_TARGET} PRIVATE LokaAppleToolboxCore)

  target_compile_definitions(${LOKA_TUTORIAL_CLASSIC_TARGET} PRIVATE LOKA_RETRO68=1 LOKA_PROFILE_FUNC_TICKS=1)

  set_target_properties(${LOKA_TUTORIAL_CLASSIC_TARGET} PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(${LOKA_TUTORIAL_CLASSIC_TARGET} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
  )
  target_link_options(${LOKA_TUTORIAL_CLASSIC_TARGET} PRIVATE
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
      add_custom_command(TARGET ${LOKA_TUTORIAL_CLASSIC_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
          -DSTRIP_TOOL="${RETRO68_STRIP}"
          -DSTRIP_INPUT="$<TARGET_FILE:${LOKA_TUTORIAL_CLASSIC_TARGET}>.gdb"
          -P "${CMAKE_SOURCE_DIR}/cmake/strip_classic.cmake"
        COMMENT "Stripping Classic debug binary"
      )
    endif()
  endif()
endif()
