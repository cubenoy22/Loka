cmake_minimum_required(VERSION 3.15)

# Toolbox builds are expected to use a separate toolchain (e.g. Retro68).
add_library(LokaAppleToolbox INTERFACE)

function(loka_configure_multiversal_headers target)
  set(_loka_multiversal_include_dir
    "${CMAKE_CURRENT_BINARY_DIR}/multiversal-compat")
  file(MAKE_DIRECTORY "${_loka_multiversal_include_dir}")

  # Universal Interfaces 3.x renamed the classic Manager headers. Generate
  # only the aliases Loka consumes so the source keeps one Toolbox vocabulary
  # while Multiversal remains an explicit build-time dependency.
  set(_loka_multiversal_header_aliases
    "Controls.h=ControlMgr.h"
    "Devices.h=DeviceMgr.h"
    "Dialogs.h=DialogMgr.h"
    "Events.h=EventMgr.h"
    "Files.h=FileMgr.h"
    "Fonts.h=FontMgr.h"
    "Memory.h=MemoryMgr.h"
    "Menus.h=MenuMgr.h"
    "Processes.h=ProcessMgr.h"
    "Quickdraw.h=QuickDraw.h"
    "Sound.h=SoundMgr.h"
    "StandardFile.h=StdFilePkg.h"
    "Timer.h=TimeMgr.h"
    "Windows.h=WindowMgr.h"
  )

  foreach(_loka_header_alias ${_loka_multiversal_header_aliases})
    string(REPLACE "=" ";" _loka_header_pair "${_loka_header_alias}")
    list(GET _loka_header_pair 0 _loka_universal_header)
    list(GET _loka_header_pair 1 _loka_multiversal_header)
    file(WRITE "${_loka_multiversal_include_dir}/${_loka_universal_header}"
      "#include <${_loka_multiversal_header}>\n")
  endforeach()

  target_include_directories(${target} BEFORE INTERFACE
    "${_loka_multiversal_include_dir}"
  )
  target_compile_definitions(${target} INTERFACE
    LOKA_TOOLBOX_MULTIVERSAL_INTERFACES=1
  )
endfunction()

set(LOKA_TOOLBOX_SOURCES
  src/ToolboxBootstrap.cpp
  src/ToolboxOperatorNew.cpp
  src/ToolboxString.cpp
  src/ToolboxApp.cpp
  src/ToolboxByteSource.cpp
  src/ToolboxNativeImage.cpp
  src/PictParser.cpp
  src/ToolboxProfiler.cpp
  src/ToolboxBuiltInSupport.cpp
  src/debug/ToolboxSceneDebugStats.cpp
  src/context/ToolboxButtonContext.cpp
  src/context/ToolboxProjectedNodeContext.cpp
  src/context/ToolboxCellContext.cpp
  src/context/ToolboxEditTextContext.cpp
  src/context/ToolboxOpenFileDialogContext.cpp
  src/context/ToolboxPopupMenuContext.cpp
  src/context/ToolboxScrollBarContext.cpp
  src/context/ToolboxTextContext.cpp
  src/context/ToolboxImageViewContext.cpp
  src/context/ToolboxRectSurfaceContext.cpp
  src/ToolboxPlatformLayoutHandlers.cpp
  src/ToolboxScenePlatformController.cpp
  src/ToolboxWindow.cpp
  src/ToolboxWindowContext.cpp
  src/ToolboxPlatformContext.cpp
  src/platform/ToolboxAppLocation.cpp
  src/platform/ToolboxFileIO.cpp
  src/platform/ToolboxHfsName.cpp
)

file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/*.cpp)

function(loka_configure_toolbox_core target)
  loka_enable_warnings(${target})
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )

  if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
    target_compile_options(${target} PRIVATE
      -Os
      -ffunction-sections
      -fdata-sections
      -fno-exceptions
      -fno-rtti
    )
    if(LOKA_TOOLBOX_CLASSIC_6)
      target_compile_definitions(${target} PRIVATE LOKA_TOOLBOX_CLASSIC_6=1)
    endif()
    # LOKA_PROFILE_FUNC_TICKS / LOKA_RETRO68_DIAGNOSTICS come from the
    # top-level LOKA_RETRO68_DIAGNOSTICS option (#135).
    target_compile_definitions(${target} PRIVATE LOKA_RETRO68=1)
    target_link_options(${target} PRIVATE
      -Wl,--gc-sections
    )
  endif()

  target_link_libraries(${target} PUBLIC
    LokaAppleToolbox
  )

  target_include_directories(${target} PUBLIC
    ${PROJECT_SOURCE_DIR}/common
  )

  target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
  )
endfunction()

add_library(LokaAppleToolboxCore STATIC ${LOKA_TOOLBOX_SOURCES} ${COMMON_SOURCES})
loka_configure_toolbox_core(LokaAppleToolboxCore)

if(CMAKE_SYSTEM_NAME MATCHES "^Retro68$")
  # Standalone scenario applications need TEST_ID-bearing inline definitions
  # to have one meaning throughout the executable. Keep this variant explicit
  # and out of ordinary builds rather than mixing it with the shipping core.
  add_library(LokaAppleToolboxTestCore STATIC EXCLUDE_FROM_ALL
    ${LOKA_TOOLBOX_SOURCES}
    ${COMMON_SOURCES}
  )
  target_compile_definitions(LokaAppleToolboxTestCore PRIVATE
    TEST_BUILD
    LOKA_DIAG=1
  )
  loka_configure_toolbox_core(LokaAppleToolboxTestCore)
endif()

target_link_libraries(LokaAppleToolbox INTERFACE
  LokaAppleCommonCarbon
)

target_include_directories(LokaAppleToolbox INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

if(LOKA_TOOLBOX_MULTIVERSAL_INTERFACES)
  loka_configure_multiversal_headers(LokaAppleToolbox)
endif()
