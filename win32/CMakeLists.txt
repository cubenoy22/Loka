cmake_minimum_required(VERSION 3.15)
project(Declara LANGUAGES CXX)

# C++98互換
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ソースファイル
file(GLOB_RECURSE COMMON_SOURCES ../common/*.cpp)
list(APPEND COMMON_SOURCES ../common/core/Profiler.cpp)
list(REMOVE_DUPLICATES COMMON_SOURCES)
set(SOURCES
  src/Win32App.cpp
  src/Win32Window.cpp
  src/Win32ScenePlatformController.cpp
  src/Win32PlatformContext.cpp
  src/MsgBoxWin32.cpp
  src/context/Win32ButtonContext.cpp
  src/context/Win32CellContext.cpp
  src/context/Win32TextContext.cpp
  src/context/Win32EditTextContext.cpp
  src/context/Win32OpenFileDialogContext.cpp
  src/context/Win32PopupMenuContext.cpp
  src/platform/Win32DebugLog.cpp
  src/platform/Win32String.cpp
)

add_library(DeclaraWin32Core STATIC ${SOURCES} ${COMMON_SOURCES})
target_include_directories(DeclaraWin32Core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../tests
)
target_compile_definitions(DeclaraWin32Core PRIVATE
  $<$<CONFIG:Debug>:LOKA_DEBUG_RECOMPOSE=1>
)

# Windows 固有の警告抑制など（必要なら追加）
if(MSVC)
    target_compile_options(DeclaraWin32Core PRIVATE /utf-8)
endif()

# アーキテクチャ切替例（x86/x64/ARM64）
# cmake -A Win32|x64|ARM64 で指定可能
# 例: cmake -G "Visual Studio 17 2022" -A ARM64 ..

# ビルドタイプ（Debug/Release）はCMAKE_BUILD_TYPEまたはIDEで選択

option(TEST_BUILD "Enable test build mode" OFF)
if(TEST_BUILD)
  add_definitions(-DTEST_BUILD)
endif()
