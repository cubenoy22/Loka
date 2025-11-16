cmake_minimum_required(VERSION 3.15)
project(DeclaraHelloWin32 LANGUAGES CXX)

set(HELLOWORLD_SOURCES
  src/main.cpp
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

# Use UTF-8 source/execution charset on MSVC to avoid codepage warnings
if(MSVC)
  target_compile_options(DeclaraHelloWin32 PRIVATE /utf-8)
endif()
