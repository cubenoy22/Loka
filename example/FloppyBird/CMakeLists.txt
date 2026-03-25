cmake_minimum_required(VERSION 3.15)
project(LokaFloppyBird LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

if(NOT WIN32 AND NOT APPLE)
  if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Retro68"))
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
  add_executable(LokaFloppyBirdMacOS
    src/MacOSMain.mm
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

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(LokaFloppyBirdMacOS PRIVATE "-fno-objc-arc")
  endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
  add_application(LokaFloppyBirdClassic
    src/main.cpp
  )

  target_include_directories(LokaFloppyBirdClassic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/toolbox/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(LokaFloppyBirdClassic PRIVATE LokaAppleToolboxCore)

  target_compile_definitions(LokaFloppyBirdClassic PRIVATE LOKA_RETRO68=1 LOKA_PROFILE_FUNC_TICKS=1)

  set_target_properties(LokaFloppyBirdClassic PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(LokaFloppyBirdClassic PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
  )
  target_link_options(LokaFloppyBirdClassic PRIVATE
    -Wl,--gc-sections
  )
endif()
