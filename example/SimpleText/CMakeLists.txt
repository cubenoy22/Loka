cmake_minimum_required(VERSION 3.15)
project(LokaSimpleText LANGUAGES CXX)

# Desktop targets are introduced in #1131 PR 7.
if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  set(LOKA_SIMPLETEXT_CLASSIC_TARGET "LokaSimpleText${LOKA_CLASSIC_TARGET_SUFFIX}")
  set(RETRO_SOURCES
    src/main.cpp
    Size.r
  )

  add_application(${LOKA_SIMPLETEXT_CLASSIC_TARGET} ${RETRO_SOURCES})
  loka_enable_warnings(${LOKA_SIMPLETEXT_CLASSIC_TARGET})

  target_include_directories(${LOKA_SIMPLETEXT_CLASSIC_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
  )

  target_link_libraries(${LOKA_SIMPLETEXT_CLASSIC_TARGET} PRIVATE LokaAppleToolboxCore)

  target_compile_definitions(${LOKA_SIMPLETEXT_CLASSIC_TARGET} PRIVATE LOKA_RETRO68=1)

  set_target_properties(${LOKA_SIMPLETEXT_CLASSIC_TARGET} PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(${LOKA_SIMPLETEXT_CLASSIC_TARGET} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
    -fno-rtti
  )
  target_link_options(${LOKA_SIMPLETEXT_CLASSIC_TARGET} PRIVATE
    -Wl,--gc-sections
  )

  # Compact shipping profile (#135): drop the MacsBug name sections from the
  # CODE resources. Elf2Mac-only (68K); the DWARF .gdb ELF keeps full symbols
  # for the MAME gdbstub workflow.
  if(CMAKE_SYSTEM_NAME STREQUAL "Retro68" AND NOT LOKA_RETRO68_DIAGNOSTICS)
    target_link_options(${LOKA_SIMPLETEXT_CLASSIC_TARGET} PRIVATE -Wl,--mac-strip-macsbug)
  endif()

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
      add_custom_command(TARGET ${LOKA_SIMPLETEXT_CLASSIC_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
          -DSTRIP_TOOL="${RETRO68_STRIP}"
          -DSTRIP_INPUT="$<TARGET_FILE:${LOKA_SIMPLETEXT_CLASSIC_TARGET}>.gdb"
          -P "${CMAKE_SOURCE_DIR}/cmake/strip_classic.cmake"
        COMMENT "Stripping Classic debug binary"
      )
    endif()
  endif()
endif()
