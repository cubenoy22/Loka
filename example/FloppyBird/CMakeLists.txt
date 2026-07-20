cmake_minimum_required(VERSION 3.15)
project(LokaFloppyBird LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
  loka_suppress_arclite_link()
endif()

if(NOT WIN32 AND NOT APPLE)
  if(NOT (CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$"))
    add_custom_target(LokaFloppyBird ALL
      COMMAND ${CMAKE_COMMAND} -E echo "LokaFloppyBird has no supported platform target here."
    )
    return()
  endif()
endif()

# ---- Win32 ----------------------------------------------------------------
if(WIN32)
  add_executable(LokaFloppyBirdWin32 WIN32
    src/main.cpp
  )

  set_target_properties(LokaFloppyBirdWin32 PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_include_directories(LokaFloppyBirdWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(LokaFloppyBirdWin32 PRIVATE LokaWin32Core)

  if(MSVC)
    target_compile_options(LokaFloppyBirdWin32 PRIVATE /utf-8)
  endif()
endif()

# ---- macOS ----------------------------------------------------------------
if(APPLE)
  file(GLOB_RECURSE MACOS_HEADERS src/*.hpp)

  add_executable(LokaFloppyBirdMacOS
    src/MacOSMain.mm
    ${MACOS_HEADERS}
  )

  set_target_properties(LokaFloppyBirdMacOS PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_include_directories(LokaFloppyBirdMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(LokaFloppyBirdMacOS PRIVATE
    LokaMacOSCore
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND LOKA_MACOS_EXPLICIT_NO_ARC_FLAGS)
    target_compile_options(LokaFloppyBirdMacOS PRIVATE "-fno-objc-arc")
  endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  set(LOKA_FLOPPYBIRD_CLASSIC_TARGET "LokaFloppyBird${LOKA_CLASSIC_TARGET_SUFFIX}")

  add_application(${LOKA_FLOPPYBIRD_CLASSIC_TARGET}
    src/main.cpp
  )

  target_include_directories(${LOKA_FLOPPYBIRD_CLASSIC_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(${LOKA_FLOPPYBIRD_CLASSIC_TARGET} PRIVATE LokaAppleToolboxCore)

  target_compile_definitions(${LOKA_FLOPPYBIRD_CLASSIC_TARGET} PRIVATE LOKA_RETRO68=1)

  set_target_properties(${LOKA_FLOPPYBIRD_CLASSIC_TARGET} PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(${LOKA_FLOPPYBIRD_CLASSIC_TARGET} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
    -fno-rtti
  )
  target_link_options(${LOKA_FLOPPYBIRD_CLASSIC_TARGET} PRIVATE
    -Wl,--gc-sections
  )

  # Compact shipping profile (#135): drop the MacsBug name sections from the
  # CODE resources. Elf2Mac-only (68K); the DWARF .gdb ELF keeps full symbols
  # for the MAME gdbstub workflow.
  if(CMAKE_SYSTEM_NAME STREQUAL "Retro68" AND NOT LOKA_RETRO68_DIAGNOSTICS)
    target_link_options(${LOKA_FLOPPYBIRD_CLASSIC_TARGET} PRIVATE -Wl,--mac-strip-macsbug)
  endif()
endif()
