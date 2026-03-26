cmake_minimum_required(VERSION 3.15)

# Toolbox builds are expected to use a separate toolchain (e.g. Retro68).
add_library(LokaAppleToolbox INTERFACE)

set(LOKA_TOOLBOX_SOURCES
  src/ToolboxString.cpp
  src/ToolboxApp.cpp
  src/ToolboxNativeImage.cpp
  src/ToolboxProfiler.cpp
  src/debug/ToolboxSceneDebugStats.cpp
  src/context/ToolboxButtonContext.cpp
  src/context/ToolboxCellContext.cpp
  src/context/ToolboxEditTextContext.cpp
  src/context/ToolboxOpenFileDialogContext.cpp
  src/context/ToolboxNodeContextMapper.cpp
  src/context/ToolboxPopupMenuContext.cpp
  src/context/ToolboxTextContext.cpp
  src/context/ToolboxImageViewContext.cpp
  src/context/ToolboxRectSurfaceContext.cpp
  src/ToolboxScenePlatformController.cpp
  src/ToolboxWindow.cpp
  src/ToolboxWindowContext.cpp
  src/ToolboxPlatformContext.cpp
)

file(GLOB_RECURSE COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.cpp)

add_library(LokaAppleToolboxCore STATIC ${LOKA_TOOLBOX_SOURCES} ${COMMON_SOURCES})

target_link_libraries(LokaAppleToolbox INTERFACE
  LokaAppleCommonCarbon
)

if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
  target_compile_options(LokaAppleToolboxCore PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
    -fno-rtti
  )
  if(LOKA_TOOLBOX_CLASSIC_6)
    target_compile_definitions(LokaAppleToolboxCore PRIVATE LOKA_TOOLBOX_CLASSIC_6=1)
  endif()
  # Function profiler (set to 1 to enable)
  target_compile_definitions(LokaAppleToolboxCore PRIVATE LOKA_PROFILE_FUNC_TICKS=1 LOKA_RETRO68=1)
  target_link_options(LokaAppleToolboxCore PRIVATE
    -Wl,--gc-sections
  )
endif()

target_link_libraries(LokaAppleToolboxCore PUBLIC
  LokaAppleToolbox
)

target_include_directories(LokaAppleToolbox INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

target_include_directories(LokaAppleToolboxCore PUBLIC
  ${PROJECT_SOURCE_DIR}/common
  ${CMAKE_CURRENT_LIST_DIR}/src
)
