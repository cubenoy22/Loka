cmake_minimum_required(VERSION 3.15)
project(Declara LANGUAGES CXX)

# C++98互換
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ソースファイル
file(GLOB_RECURSE COMMON_SOURCES ../common/*.cpp)
set(SOURCES
  src/main.cpp
  src/Win32App.cpp
  src/Win32Window.cpp
  src/Win32ScenePlatformController.cpp
  src/Win32PlatformContext.cpp
  src/Tests.cpp
  ${COMMON_SOURCES}
)

option(USE_CONSOLE_SUBSYSTEM "Use console subsystem (main entrypoint)" OFF)
if(USE_CONSOLE_SUBSYSTEM)
  add_executable(Declara ${SOURCES})
else()
  add_executable(Declara WIN32 ${SOURCES})
endif()

# 出力バイナリ名を明示
set_target_properties(Declara PROPERTIES OUTPUT_NAME "Declara")

# Windows サブシステム指定（GUIアプリ用）
option(USE_CONSOLE_SUBSYSTEM "Use console subsystem (main entrypoint)" OFF)
if(MSVC)
    if(USE_CONSOLE_SUBSYSTEM)
        set_target_properties(Declara PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    else()
        set_target_properties(Declara PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
    endif()
    target_compile_options(Declara PRIVATE /utf-8)
endif()

# インクルードパスにcommon/を追加
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# アーキテクチャ切替例（x86/x64/ARM64）
# cmake -A Win32|x64|ARM64 で指定可能
# 例: cmake -G "Visual Studio 17 2022" -A ARM64 ..

# ビルドタイプ（Debug/Release）はCMAKE_BUILD_TYPEまたはIDEで選択

option(TEST_BUILD "Enable test build mode" OFF)
if(TEST_BUILD)
  add_definitions(-DTEST_BUILD)
endif()
