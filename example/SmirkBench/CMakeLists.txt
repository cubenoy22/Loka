cmake_minimum_required(VERSION 3.15)
project(LokaSmirkBench LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
  loka_suppress_arclite_link()
endif()

if(NOT WIN32 AND NOT APPLE)
  if(NOT (CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$"))
    add_custom_target(LokaSmirkBench ALL
      COMMAND ${CMAKE_COMMAND} -E echo "LokaSmirkBench has no supported platform target here."
    )
    return()
  endif()
endif()

if(WIN32)
  include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/LokaWin32VisualStyles.cmake)
  add_executable(LokaSmirkBenchWin32 WIN32
    src/main.cpp
  )
  loka_enable_warnings(LokaSmirkBenchWin32)
  loka_enable_win32_visual_styles(LokaSmirkBenchWin32)

  set_target_properties(LokaSmirkBenchWin32 PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_include_directories(LokaSmirkBenchWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(LokaSmirkBenchWin32 PRIVATE LokaWin32Core)

  if(MSVC)
    target_compile_options(LokaSmirkBenchWin32 PRIVATE /utf-8)
  endif()
endif()

if(APPLE)
  file(GLOB_RECURSE MACOS_HEADERS src/*.hpp)

  add_executable(LokaSmirkBenchMacOS
    src/MacOSMain.mm
    ${MACOS_HEADERS}
  )
  loka_enable_warnings(LokaSmirkBenchMacOS)

  set_target_properties(LokaSmirkBenchMacOS PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_include_directories(LokaSmirkBenchMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(LokaSmirkBenchMacOS PRIVATE LokaMacOSCore)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND LOKA_MACOS_EXPLICIT_NO_ARC_FLAGS)
    target_compile_options(LokaSmirkBenchMacOS PRIVATE "-fno-objc-arc")
  endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  set(LOKA_SMIRKBENCH_CLASSIC_TARGET "LokaSmirkBench${LOKA_CLASSIC_TARGET_SUFFIX}")

  add_application(${LOKA_SMIRKBENCH_CLASSIC_TARGET}
    src/main.cpp
    Size.r
  )
  loka_enable_warnings(${LOKA_SMIRKBENCH_CLASSIC_TARGET})

  target_include_directories(${LOKA_SMIRKBENCH_CLASSIC_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(${LOKA_SMIRKBENCH_CLASSIC_TARGET} PRIVATE LokaAppleToolboxCore)

  target_compile_definitions(${LOKA_SMIRKBENCH_CLASSIC_TARGET} PRIVATE LOKA_RETRO68=1)

  set_target_properties(${LOKA_SMIRKBENCH_CLASSIC_TARGET} PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  target_compile_options(${LOKA_SMIRKBENCH_CLASSIC_TARGET} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
    -fno-rtti
  )
  target_link_options(${LOKA_SMIRKBENCH_CLASSIC_TARGET} PRIVATE
    -Wl,--gc-sections
  )

  if(CMAKE_SYSTEM_NAME STREQUAL "Retro68" AND NOT LOKA_RETRO68_DIAGNOSTICS)
    target_link_options(${LOKA_SMIRKBENCH_CLASSIC_TARGET} PRIVATE -Wl,--mac-strip-macsbug)
  endif()
endif()
