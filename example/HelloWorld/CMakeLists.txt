cmake_minimum_required(VERSION 3.15)
project(DeclaraHelloWorld LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

if(NOT WIN32 AND NOT APPLE AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Retro68"))
  add_custom_target(DeclaraHelloWorld ALL
    COMMAND ${CMAKE_COMMAND} -E echo "DeclaraHelloWorld has no supported platform target here."
  )
  return()
endif()

if(WIN32)
  set(HELLOWORLD_SOURCES
    src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/Tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/TestMain.cpp
  )

  if(NOT TARGET DeclaraWin32Core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../win32 ${CMAKE_CURRENT_BINARY_DIR}/win32)
  endif()

  # Option to use console subsystem (main) instead of GUI subsystem (WinMain)
  option(USE_CONSOLE_SUBSYSTEM "Use console subsystem for debugging" OFF)

  if(USE_CONSOLE_SUBSYSTEM)
    add_executable(DeclaraHelloWin32 ${HELLOWORLD_SOURCES})
  else()
    add_executable(DeclaraHelloWin32 WIN32 ${HELLOWORLD_SOURCES})
  endif()

  target_include_directories(DeclaraHelloWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
  )

  target_link_libraries(DeclaraHelloWin32 PRIVATE DeclaraWin32Core)
  target_compile_definitions(DeclaraHelloWin32 PRIVATE
    $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
  )

  # Use UTF-8 source/execution charset on MSVC to avoid codepage warnings
  if(MSVC)
    target_compile_options(DeclaraHelloWin32 PRIVATE /utf-8)
  endif()

  # Test target (console app)
  set(WIN32_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/Tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/TestMain.cpp
  )

  add_executable(DeclaraTestsWin32 ${WIN32_TEST_SOURCES})

  target_include_directories(DeclaraTestsWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
  )

  target_link_libraries(DeclaraTestsWin32 PRIVATE DeclaraWin32Core)
  target_compile_definitions(DeclaraTestsWin32 PRIVATE
    $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
  )

  if(MSVC)
    target_compile_options(DeclaraTestsWin32 PRIVATE /utf-8)
  endif()
endif()

if(APPLE)
  set(MACOS_SOURCES
    src/MacOSMain.mm
  )

  add_executable(DeclaraHelloMacOS ${MACOS_SOURCES})

  target_include_directories(DeclaraHelloMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
  )

  target_link_libraries(DeclaraHelloMacOS PRIVATE DeclaraMacOSCore)
  target_compile_options(DeclaraHelloMacOS PRIVATE "-fobjc-arc")
  target_compile_definitions(DeclaraHelloMacOS PRIVATE
    $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
  )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
  set(RETRO_SOURCES
    src/main.cpp
  )

  add_application(DeclaraHelloClassic ${RETRO_SOURCES})

  target_include_directories(DeclaraHelloClassic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/toolbox/src
  )

  target_link_libraries(DeclaraHelloClassic PRIVATE DeclaraAppleToolboxCore)

  target_compile_definitions(DeclaraHelloClassic PRIVATE LOKA_RETRO68=1 LOKA_PROFILE_FUNC_TICKS=0)

  set_target_properties(DeclaraHelloClassic PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(DeclaraHelloClassic PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
  )
  target_link_options(DeclaraHelloClassic PRIVATE
    -Wl,--gc-sections
  )

  option(DECLARA_CLASSIC_STRIP "Strip Classic Mac binaries after link" ON)
  if(DECLARA_CLASSIC_STRIP)
    find_program(RETRO68_STRIP
      NAMES m68k-apple-macos-strip powerpc-apple-macos-strip
      HINTS
        "$ENV{RETRO68_BUILD_DIR}/toolchain/bin"
        "${RETRO68_BUILD_DIR}/toolchain/bin"
        "${RETRO68_TOOLCHAIN_DIR}/../bin"
    )
    if(RETRO68_STRIP)
      add_custom_command(TARGET DeclaraHelloClassic POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
          -DSTRIP_TOOL="${RETRO68_STRIP}"
          -DSTRIP_INPUT="$<TARGET_FILE:DeclaraHelloClassic>.gdb"
          -P "${CMAKE_SOURCE_DIR}/cmake/strip_classic.cmake"
        COMMENT "Stripping Classic debug binary"
      )
    endif()
  endif()
endif()
