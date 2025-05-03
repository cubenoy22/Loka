cmake_minimum_required(VERSION 3.15)
project(Declara LANGUAGES C)

# ソース自動収集（main.c + 将来のcommon/*.c）
file(GLOB WIN32_SOURCES
    src/main.c
    ../common/*.c
)

add_executable(Declara ${WIN32_SOURCES})

# 出力バイナリ名を明示
set_target_properties(Declara PROPERTIES OUTPUT_NAME "Declara")

# Windows サブシステム指定（GUIアプリ用）
if(MSVC)
    set_target_properties(Declara PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif()

# インクルードパスにcommon/を追加
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# アーキテクチャ切替例（x86/x64/ARM64）
# cmake -A Win32|x64|ARM64 で指定可能
# 例: cmake -G "Visual Studio 17 2022" -A ARM64 ..

# ビルドタイプ（Debug/Release）はCMAKE_BUILD_TYPEまたはIDEで選択
