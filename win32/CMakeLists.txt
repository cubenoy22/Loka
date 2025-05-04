cmake_minimum_required(VERSION 3.15)
project(Declara LANGUAGES CXX)

# C++98互換
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ソースファイル
set(SOURCES
  src/main.cpp
  src/Win32App.cpp       # Win32App.cppを追加
  src/Win32Window.cpp    # Win32Window.cppも追加
  ../common/main.cpp
  ../common/State.cpp
)

add_executable(Declara WIN32 ${SOURCES})

# 出力バイナリ名を明示
set_target_properties(Declara PROPERTIES OUTPUT_NAME "Declara")

# Windows サブシステム指定（GUIアプリ用）
if(MSVC)
    set_target_properties(Declara PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
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
