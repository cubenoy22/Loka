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
  add_custom_target(LokaStaticVsDynamicClassic ALL
    COMMAND ${CMAKE_COMMAND} -E echo "LokaStaticVsDynamicClassic is not implemented yet."
  )
endif()
