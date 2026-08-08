cmake_minimum_required(VERSION 3.15)
project(ScrapbookUI LANGUAGES CXX)

if(NOT WIN32 AND NOT (CMAKE_SYSTEM_NAME MATCHES "^Retro68$"))
  add_custom_target(ScrapbookUI ALL
    COMMAND ${CMAKE_COMMAND} -E echo
      "ScrapbookUI has no supported platform target here."
  )
  return()
endif()

set(SCRAPBOOK_COMMON_SOURCES
  src/main.cpp
  src/ScrapbookPackage.cpp
)

if(WIN32)
  if(NOT TARGET LokaWin32Core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../win32 ${CMAKE_CURRENT_BINARY_DIR}/win32)
  endif()

  add_executable(ScrapbookUIWin32 WIN32 ${SCRAPBOOK_COMMON_SOURCES})
  loka_enable_warnings(ScrapbookUIWin32)

  target_include_directories(ScrapbookUIWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
  )
  target_link_libraries(ScrapbookUIWin32 PRIVATE LokaWin32Core)
  set_target_properties(ScrapbookUIWin32 PROPERTIES
    CXX_STANDARD 98
    CXX_STANDARD_REQUIRED YES
  )
  if(MSVC)
    target_compile_options(ScrapbookUIWin32 PRIVATE /utf-8)
  endif()

  # The source package is environment-specific; the runtime contract is not.
  configure_file(assets/ASSETS-modern.LRP ASSETS.LRP COPYONLY)
endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "^Retro68$")
  return()
endif()

set(SCRAPBOOK_CLASSIC_TARGET "ScrapbookUI${LOKA_CLASSIC_TARGET_SUFFIX}")
set(SCRAPBOOK_SOURCES
  ${SCRAPBOOK_COMMON_SOURCES}
  Size.r
)

add_application(${SCRAPBOOK_CLASSIC_TARGET} ${SCRAPBOOK_SOURCES})
loka_enable_warnings(${SCRAPBOOK_CLASSIC_TARGET})

target_include_directories(${SCRAPBOOK_CLASSIC_TARGET} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../common
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/toolbox/src
)

target_link_libraries(${SCRAPBOOK_CLASSIC_TARGET} PRIVATE
  LokaAppleToolboxCore
)

target_compile_definitions(${SCRAPBOOK_CLASSIC_TARGET} PRIVATE
  LOKA_RETRO68=1
)

set(LOKA_SCRAPBOOK_PAGE_COUNT "" CACHE STRING
  "Override ScrapbookUI's committed package page count")
set(LOKA_SCRAPBOOK_ID_SPACE_STAMP "" CACHE STRING
  "Override ScrapbookUI's committed package id-space stamp")

if(NOT LOKA_SCRAPBOOK_PAGE_COUNT STREQUAL "")
  target_compile_definitions(${SCRAPBOOK_CLASSIC_TARGET} PRIVATE
    LOKA_SCRAPBOOK_PAGE_COUNT=${LOKA_SCRAPBOOK_PAGE_COUNT}
  )
endif()

if(NOT LOKA_SCRAPBOOK_ID_SPACE_STAMP STREQUAL "")
  target_compile_definitions(${SCRAPBOOK_CLASSIC_TARGET} PRIVATE
    LOKA_SCRAPBOOK_ID_SPACE_STAMP=${LOKA_SCRAPBOOK_ID_SPACE_STAMP}
  )
endif()

set_target_properties(${SCRAPBOOK_CLASSIC_TARGET} PROPERTIES
  CXX_STANDARD 98
  CXX_STANDARD_REQUIRED YES
)

target_compile_options(${SCRAPBOOK_CLASSIC_TARGET} PRIVATE
  -Os
  -ffunction-sections
  -fdata-sections
  -fno-exceptions
  -fno-rtti
)
target_link_options(${SCRAPBOOK_CLASSIC_TARGET} PRIVATE
  -Wl,--gc-sections
)

if(NOT LOKA_RETRO68_DIAGNOSTICS)
  target_link_options(${SCRAPBOOK_CLASSIC_TARGET} PRIVATE
    -Wl,--mac-strip-macsbug
  )
endif()

# Keep the committed package beside the APPL/dsk outputs for the development
# disk script. Packing remains an explicit host-side regeneration step.
configure_file(ASSETS.LRP ASSETS.LRP COPYONLY)
