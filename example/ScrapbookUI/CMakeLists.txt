cmake_minimum_required(VERSION 3.15)
project(ScrapbookUI LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME MATCHES "^Retro68$")
  add_custom_target(ScrapbookUI ALL
    COMMAND ${CMAKE_COMMAND} -E echo
      "ScrapbookUI is Retro68-only. No app is built for this platform."
  )
  return()
endif()

set(SCRAPBOOK_CLASSIC_TARGET "ScrapbookUI${LOKA_CLASSIC_TARGET_SUFFIX}")
set(SCRAPBOOK_SOURCES
  src/main.cpp
  src/ScrapbookPackage.cpp
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
