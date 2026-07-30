cmake_minimum_required(VERSION 3.15)
project(LokaTestsToolbox LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME MATCHES "^Retro68$")
  add_custom_target(LokaTestsToolbox ALL
    COMMAND ${CMAKE_COMMAND} -E echo
      "LokaTestsToolbox is Retro68-only. No app is built for this platform."
  )
  return()
endif()

set(LOKA_TESTS_TOOLBOX_TARGET "LokaTestsToolbox${LOKA_CLASSIC_TARGET_SUFFIX}")
set(LOKA_TESTS_TOOLBOX_SOURCES
  src/main.cpp
  src/ScenarioDriver.cpp
  src/ScrapbookScenarios.cpp
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/src/ScrapbookPackage.cpp
  ${CMAKE_SOURCE_DIR}/common/testing/snap/SnapFormat.cpp
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/Size.r
)

add_application(${LOKA_TESTS_TOOLBOX_TARGET} ${LOKA_TESTS_TOOLBOX_SOURCES})
loka_enable_warnings(${LOKA_TESTS_TOOLBOX_TARGET})

target_include_directories(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/src
  ${CMAKE_SOURCE_DIR}/common
  ${CMAKE_SOURCE_DIR}/apple/toolbox/src
)

target_link_libraries(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
  LokaAppleToolboxCore
)

target_compile_definitions(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
  LOKA_RETRO68=1
)

if(NOT LOKA_SCRAPBOOK_PAGE_COUNT STREQUAL "")
  target_compile_definitions(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
    LOKA_SCRAPBOOK_PAGE_COUNT=${LOKA_SCRAPBOOK_PAGE_COUNT}
  )
endif()

if(NOT LOKA_SCRAPBOOK_ID_SPACE_STAMP STREQUAL "")
  target_compile_definitions(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
    LOKA_SCRAPBOOK_ID_SPACE_STAMP=${LOKA_SCRAPBOOK_ID_SPACE_STAMP}
  )
endif()

set_target_properties(${LOKA_TESTS_TOOLBOX_TARGET} PROPERTIES
  CXX_STANDARD 98
  CXX_STANDARD_REQUIRED YES
)

target_compile_options(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
  -Os
  -ffunction-sections
  -fdata-sections
  -fno-exceptions
  -fno-rtti
)
target_link_options(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
  -Wl,--gc-sections
)

if(NOT LOKA_RETRO68_DIAGNOSTICS)
  target_link_options(${LOKA_TESTS_TOOLBOX_TARGET} PRIVATE
    -Wl,--mac-strip-macsbug
  )
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/example/ScrapbookUI/ASSETS.LRP
  ASSETS.LRP
  COPYONLY
)
