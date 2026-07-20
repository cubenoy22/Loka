cmake_minimum_required(VERSION 3.15)

# Toolbox builds are expected to use a separate toolchain (e.g. Retro68).
add_library(LokaAppleToolbox INTERFACE)

set(LOKA_TOOLBOX_SOURCES
  src/ToolboxBootstrap.cpp
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
  src/ToolboxPlatformLayoutHandlers.cpp
  src/ToolboxScenePlatformController.cpp
  src/ToolboxWindow.cpp
  src/ToolboxWindowContext.cpp
  src/ToolboxPlatformContext.cpp
)

file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/*.cpp)

add_library(LokaAppleToolboxCore STATIC ${LOKA_TOOLBOX_SOURCES} ${COMMON_SOURCES})
set_target_properties(LokaAppleToolboxCore PROPERTIES
  CXX_STANDARD 98
  CXX_STANDARD_REQUIRED YES
)

target_link_libraries(LokaAppleToolbox INTERFACE
  LokaAppleCommonCarbon
)

if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
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
  # LOKA_PROFILE_FUNC_TICKS / LOKA_RETRO68_DIAGNOSTICS come from the
  # top-level LOKA_RETRO68_DIAGNOSTICS option (#135).
  target_compile_definitions(LokaAppleToolboxCore PRIVATE LOKA_RETRO68=1)
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
)

target_include_directories(LokaAppleToolboxCore PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src
)
