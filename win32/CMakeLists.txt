set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Standalone example projects add this platform directory without the root.
if(NOT COMMAND loka_enable_warnings)
  include("${CMAKE_CURRENT_LIST_DIR}/../cmake/LokaWarnings.cmake")
endif()

file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS ../common/*.cpp)
list(REMOVE_DUPLICATES COMMON_SOURCES)
set(SOURCES
  src/Win32Bootstrap.cpp
  src/Win32App.cpp
  src/Win32BuiltInSupport.cpp
  src/Win32Window.cpp
  src/Win32ScenePlatformController.cpp
  src/Win32PlatformContext.cpp
  src/context/Win32ButtonContext.cpp
  src/context/Win32RetirableContext.cpp
  src/context/Win32CellContext.cpp
  src/context/Win32TextContext.cpp
  src/context/Win32EditTextContext.cpp
  src/context/Win32OpenFileDialogContext.cpp
  src/context/Win32PopupMenuContext.cpp
  src/context/Win32ImageViewContext.cpp
  src/context/Win32RectSurfaceContext.cpp
  src/platform/Win32DebugLog.cpp
  src/platform/Win32Profiler.cpp
  src/platform/Win32String.cpp
  src/platform/Win32FileIO.cpp
  src/platform/Win32AppLocation.cpp
)

add_library(LokaWin32Core STATIC ${SOURCES} ${COMMON_SOURCES})
loka_enable_warnings(LokaWin32Core)
target_include_directories(LokaWin32Core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_include_directories(LokaWin32Core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(LokaWin32Core PRIVATE windowscodecs ole32)

if(MSVC)
    target_compile_options(LokaWin32Core PRIVATE /utf-8)
endif()

get_filename_component(LOKA_WIN32_REPOSITORY_ROOT
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ABSOLUTE
)

if(CMAKE_SOURCE_DIR STREQUAL LOKA_WIN32_REPOSITORY_ROOT)
  include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/LokaWin32VisualStyles.cmake)

  set(LOKA_STANDALONE_FLOW_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/../example/ScrapbookUI/assets/ASSETS-modern.LRP"
  )
  set(LOKA_STANDALONE_FLOW_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/example/ScrapbookUI/standalone-flow"
  )

  # TEST-only presentation target. Compile common/platform sources uniformly
  # with TEST_BUILD rather than mixing the shipping core's Node layout with the
  # TEST_ID-bearing scenario layout.
  add_executable(LokaScrapbookStandaloneFlowWin32 WIN32 EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/win32/StandaloneFlowMain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/standalone/StandaloneFlowApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scenarios/ScrapbookScenarios.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/TestingHooks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../example/ScrapbookUI/src/ScrapbookPackage.cpp
    ${SOURCES}
    ${COMMON_SOURCES}
  )
  loka_enable_warnings(LokaScrapbookStandaloneFlowWin32)
  loka_enable_win32_visual_styles(LokaScrapbookStandaloneFlowWin32)

  target_include_directories(LokaScrapbookStandaloneFlowWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scenarios
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/standalone
    ${CMAKE_CURRENT_SOURCE_DIR}/../example/ScrapbookUI/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_compile_definitions(LokaScrapbookStandaloneFlowWin32 PRIVATE TEST_BUILD)
  target_link_libraries(LokaScrapbookStandaloneFlowWin32 PRIVATE windowscodecs ole32)
  set_target_properties(LokaScrapbookStandaloneFlowWin32 PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
    RUNTIME_OUTPUT_DIRECTORY "${LOKA_STANDALONE_FLOW_OUTPUT_DIRECTORY}"
  )

  set(LOKA_STANDALONE_FLOW_ASSET_OUTPUT_DIRECTORY
    "${LOKA_STANDALONE_FLOW_OUTPUT_DIRECTORY}"
  )
  if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generators append the selected configuration to the
    # runtime directory. Keep the sidecar on the same target-owned path.
    set(LOKA_STANDALONE_FLOW_ASSET_OUTPUT_DIRECTORY
      "${LOKA_STANDALONE_FLOW_OUTPUT_DIRECTORY}/$<CONFIG>"
    )
  endif()

  add_custom_target(LokaScrapbookStandaloneFlowWin32Assets
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "${LOKA_STANDALONE_FLOW_ASSET_OUTPUT_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${LOKA_STANDALONE_FLOW_ASSETS}"
      "${LOKA_STANDALONE_FLOW_ASSET_OUTPUT_DIRECTORY}/ASSETS.LRP"
    DEPENDS "${LOKA_STANDALONE_FLOW_ASSETS}"
    VERBATIM
  )
  add_dependencies(
    LokaScrapbookStandaloneFlowWin32
    LokaScrapbookStandaloneFlowWin32Assets
  )

  if(MSVC)
    target_compile_options(LokaScrapbookStandaloneFlowWin32 PRIVATE /utf-8)
  endif()

  unset(LOKA_STANDALONE_FLOW_OUTPUT_DIRECTORY)
  unset(LOKA_STANDALONE_FLOW_ASSET_OUTPUT_DIRECTORY)
  unset(LOKA_STANDALONE_FLOW_ASSETS)
endif()

unset(LOKA_WIN32_REPOSITORY_ROOT)
