cmake_minimum_required(VERSION 3.15)

# Toolbox builds are expected to use a separate toolchain (e.g. Retro68).
add_library(DeclaraAppleToolbox INTERFACE)

set(DECLARA_TOOLBOX_SOURCES
  src/ToolboxString.cpp
  src/ToolboxApp.cpp
  src/ToolboxProfiler.cpp
  src/context/ToolboxButtonContext.cpp
  src/context/ToolboxEditTextContext.cpp
  src/context/ToolboxNodeContextMapper.cpp
  src/context/ToolboxPopupMenuContext.cpp
  src/context/ToolboxTextContext.cpp
  src/ToolboxScenePlatformController.cpp
  src/ToolboxWindow.cpp
  src/ToolboxWindowContext.cpp
  src/ToolboxPlatformContext.cpp
)

file(GLOB_RECURSE COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.cpp)

add_library(DeclaraAppleToolboxCore STATIC ${DECLARA_TOOLBOX_SOURCES} ${COMMON_SOURCES})

target_link_libraries(DeclaraAppleToolbox INTERFACE
  DeclaraAppleCommonCarbon
)

if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
  target_compile_options(DeclaraAppleToolboxCore PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
  )
  if(DECLARA_TOOLBOX_CLASSIC_6)
    target_compile_definitions(DeclaraAppleToolboxCore PRIVATE DECLARA_TOOLBOX_CLASSIC_6=1)
  endif()
  target_link_options(DeclaraAppleToolboxCore PRIVATE
    -Wl,--gc-sections
  )
endif()

target_link_libraries(DeclaraAppleToolboxCore PUBLIC
  DeclaraAppleToolbox
)

target_include_directories(DeclaraAppleToolbox INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

target_include_directories(DeclaraAppleToolboxCore PUBLIC
  ${PROJECT_SOURCE_DIR}/common
  ${CMAKE_CURRENT_LIST_DIR}/src
)
