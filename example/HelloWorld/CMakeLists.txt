cmake_minimum_required(VERSION 3.15)
project(LokaHelloWorld LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

if(NOT WIN32 AND NOT APPLE AND NOT (CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$"))
  add_custom_target(LokaHelloWorld ALL
    COMMAND ${CMAKE_COMMAND} -E echo "LokaHelloWorld has no supported platform target here."
  )
  return()
endif()

if(WIN32)
  set(HELLOWORLD_SOURCES
    src/main.cpp
    src/MainNode.cpp
  )

  if(NOT TARGET LokaWin32Core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../win32 ${CMAKE_CURRENT_BINARY_DIR}/win32)
  endif()

  # Option to use console subsystem (main) instead of GUI subsystem (WinMain)
  option(USE_CONSOLE_SUBSYSTEM "Use console subsystem for debugging" OFF)

  if(USE_CONSOLE_SUBSYSTEM)
    add_executable(LokaHelloWin32 ${HELLOWORLD_SOURCES})
  else()
    add_executable(LokaHelloWin32 WIN32 ${HELLOWORLD_SOURCES})
  endif()

  target_include_directories(LokaHelloWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
  )

  target_link_libraries(LokaHelloWin32 PRIVATE LokaWin32Core)

  # Use UTF-8 source/execution charset on MSVC to avoid codepage warnings
  if(MSVC)
    target_compile_options(LokaHelloWin32 PRIVATE /utf-8)
  endif()

  set(WIN32_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/StateNotifyTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/SnapFormatTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/FlowDslTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/AttrDslTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/StartupRedrawTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/FlowDslTestMain.cpp
    src/MainNode.cpp
  )

  add_executable(LokaTestsWin32 ${WIN32_TEST_SOURCES})

  target_include_directories(LokaTestsWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(LokaTestsWin32 PRIVATE LokaWin32Core)
  target_compile_definitions(LokaTestsWin32 PRIVATE TEST_BUILD)

  if(MSVC)
    target_compile_options(LokaTestsWin32 PRIVATE /utf-8)
  endif()

endif()

if(APPLE)
  set(MACOS_SOURCES
    src/MacOSMain.mm
    src/MainNode.cpp
  )

  add_executable(LokaHelloMacOS ${MACOS_SOURCES})

  target_include_directories(LokaHelloMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
  )

  target_link_libraries(LokaHelloMacOS PRIVATE LokaMacOSCore)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(LokaHelloMacOS PRIVATE "-fno-objc-arc")
  endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  set(LOKA_HELLO_CLASSIC_TARGET "LokaHello${LOKA_CLASSIC_TARGET_SUFFIX}")
  set(RETRO_SOURCES
    src/main.cpp
    src/MainNode.cpp
  )

  add_application(${LOKA_HELLO_CLASSIC_TARGET} ${RETRO_SOURCES})

  target_include_directories(${LOKA_HELLO_CLASSIC_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/toolbox/src
  )

  target_link_libraries(${LOKA_HELLO_CLASSIC_TARGET} PRIVATE LokaAppleToolboxCore)

  target_compile_definitions(${LOKA_HELLO_CLASSIC_TARGET} PRIVATE LOKA_RETRO68=1 LOKA_PROFILE_FUNC_TICKS=1)

  set_target_properties(${LOKA_HELLO_CLASSIC_TARGET} PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(${LOKA_HELLO_CLASSIC_TARGET} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
  )
  target_link_options(${LOKA_HELLO_CLASSIC_TARGET} PRIVATE
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
      add_custom_command(TARGET ${LOKA_HELLO_CLASSIC_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
          -DSTRIP_TOOL="${RETRO68_STRIP}"
          -DSTRIP_INPUT="$<TARGET_FILE:${LOKA_HELLO_CLASSIC_TARGET}>.gdb"
          -P "${CMAKE_SOURCE_DIR}/cmake/strip_classic.cmake"
        COMMENT "Stripping Classic debug binary"
      )
    endif()
  endif()
endif()
