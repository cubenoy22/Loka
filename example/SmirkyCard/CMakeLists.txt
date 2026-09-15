cmake_minimum_required(VERSION 3.18)

include(cmake/QuickJS.cmake)

# A C-only seam keeps quickjs.h's modern macros out of Loka's C++98 code.
add_library(smirkycard_script STATIC src/ScriptEngine.c)
target_link_libraries(smirkycard_script PRIVATE smirkycard_quickjs)
target_include_directories(smirkycard_script PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(smirkycard_script PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
  target_compile_options(smirkycard_script PRIVATE /W4 /wd4996 /utf-8)
  if(LOKA_WARNINGS_AS_ERRORS)
    target_compile_options(smirkycard_script PRIVATE /WX)
  endif()
else()
  target_compile_options(smirkycard_script PRIVATE -Wall -Wextra)
  if(LOKA_WARNINGS_AS_ERRORS)
    target_compile_options(smirkycard_script PRIVATE -Werror)
  endif()
endif()

if(LOKA_CLASSIC_MAC)
  target_compile_definitions(smirkycard_script PRIVATE LOKA_RETRO68=1)
endif()

function(smirkycard_cpp_target target)
  loka_enable_warnings(${target})
  set_target_properties(${target} PROPERTIES CXX_STANDARD 98 CXX_STANDARD_REQUIRED YES)
  target_link_libraries(${target} PRIVATE smirkycard_script)
  target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/common)
  if(MSVC)
    target_compile_options(${target} PRIVATE /utf-8)
  endif()
endfunction()

if(WIN32)
  include(${PROJECT_SOURCE_DIR}/cmake/LokaWin32VisualStyles.cmake)
  add_executable(LokaSmirkyCardWin32 WIN32 src/main.cpp)
  smirkycard_cpp_target(LokaSmirkyCardWin32)
  loka_enable_win32_visual_styles(LokaSmirkyCardWin32)
  target_link_libraries(LokaSmirkyCardWin32 PRIVATE LokaWin32Core)
elseif(LOKA_MACOS AND LOKA_ENABLE_APPLE)
  enable_language(OBJCXX)
  loka_suppress_arclite_link()
  add_executable(LokaSmirkyCardMacOS src/MacOSMain.mm)
  smirkycard_cpp_target(LokaSmirkyCardMacOS)
  target_link_libraries(LokaSmirkyCardMacOS PRIVATE LokaMacOSCore)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND LOKA_MACOS_EXPLICIT_NO_ARC_FLAGS)
    target_compile_options(LokaSmirkyCardMacOS PRIVATE -fno-objc-arc)
  endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  set(LOKA_SMIRKYCARD_CLASSIC_TARGET "LokaSmirkyCard${LOKA_CLASSIC_TARGET_SUFFIX}")
  add_application(${LOKA_SMIRKYCARD_CLASSIC_TARGET}
    src/ToolboxMain.cpp
    Size.r
  )
  smirkycard_cpp_target(${LOKA_SMIRKYCARD_CLASSIC_TARGET})
  target_link_libraries(${LOKA_SMIRKYCARD_CLASSIC_TARGET} PRIVATE LokaAppleToolboxCore)
  target_compile_definitions(${LOKA_SMIRKYCARD_CLASSIC_TARGET} PRIVATE LOKA_RETRO68=1)
  target_compile_options(${LOKA_SMIRKYCARD_CLASSIC_TARGET} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
    -fno-rtti
  )
  target_link_options(${LOKA_SMIRKYCARD_CLASSIC_TARGET} PRIVATE -Wl,--gc-sections)
  if(CMAKE_SYSTEM_NAME STREQUAL "Retro68" AND NOT LOKA_RETRO68_DIAGNOSTICS)
    target_link_options(${LOKA_SMIRKYCARD_CLASSIC_TARGET} PRIVATE -Wl,--mac-strip-macsbug)
  endif()
endif()

if(TARGET LokaCommonTesting)
  set(smirkycard_null_sources ${LOKA_SHARED_TEST_SOURCES})
  list(FILTER smirkycard_null_sources INCLUDE REGEX "/tests/platform/null/.*\\.cpp$")
  add_executable(LokaSmirkyCardTests
    tests/SmirkyCardTests.cpp
    ${PROJECT_SOURCE_DIR}/tests/TestingHooks.cpp
    ${smirkycard_null_sources}
  )
  smirkycard_cpp_target(LokaSmirkyCardTests)
  target_link_libraries(LokaSmirkyCardTests PRIVATE LokaCommonTesting)
  add_test(NAME smirkyCardSceneSwitch COMMAND LokaSmirkyCardTests)
endif()
