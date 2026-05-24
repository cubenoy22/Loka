set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS ../common/*.cpp)
list(REMOVE_DUPLICATES COMMON_SOURCES)
set(SOURCES
  src/Win32App.cpp
  src/Win32BuiltInSupport.cpp
  src/Win32Window.cpp
  src/Win32ScenePlatformController.cpp
  src/Win32PlatformContext.cpp
  src/context/Win32ButtonContext.cpp
  src/context/Win32CellContext.cpp
  src/context/Win32TextContext.cpp
  src/context/Win32EditTextContext.cpp
  src/context/Win32OpenFileDialogContext.cpp
  src/context/Win32PopupMenuContext.cpp
  src/context/Win32ImageViewContext.cpp
  src/context/Win32RectSurfaceContext.cpp
  src/context/Win32NodeContextMapper.cpp
  src/platform/Win32DebugLog.cpp
  src/platform/Win32Profiler.cpp
  src/platform/Win32String.cpp
)

add_library(LokaWin32Core STATIC ${SOURCES} ${COMMON_SOURCES})
target_include_directories(LokaWin32Core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(LokaWin32Core PRIVATE windowscodecs ole32)

if(MSVC)
    target_compile_options(LokaWin32Core PRIVATE /utf-8)
endif()
