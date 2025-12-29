cmake_minimum_required(VERSION 3.15)
project(DeclaraHelloWorld LANGUAGES CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

if(NOT WIN32 AND NOT APPLE)
  add_custom_target(DeclaraHelloWorld ALL
    COMMAND ${CMAKE_COMMAND} -E echo "DeclaraHelloWorld has no supported platform target here."
  )
  return()
endif()

if(WIN32)
  set(HELLOWORLD_SOURCES
    src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/Tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/TestMain.cpp
  )

  if(NOT TARGET DeclaraWin32Core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../win32 ${CMAKE_CURRENT_BINARY_DIR}/win32)
  endif()

  # Option to use console subsystem (main) instead of GUI subsystem (WinMain)
  option(USE_CONSOLE_SUBSYSTEM "Use console subsystem for debugging" OFF)

  if(USE_CONSOLE_SUBSYSTEM)
    add_executable(DeclaraHelloWin32 ${HELLOWORLD_SOURCES})
  else()
    add_executable(DeclaraHelloWin32 WIN32 ${HELLOWORLD_SOURCES})
  endif()

  target_include_directories(DeclaraHelloWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
  )

  target_link_libraries(DeclaraHelloWin32 PRIVATE DeclaraWin32Core)

  # Use UTF-8 source/execution charset on MSVC to avoid codepage warnings
  if(MSVC)
    target_compile_options(DeclaraHelloWin32 PRIVATE /utf-8)
  endif()

  # Test target (console app)
  set(WIN32_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/Tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/TestMain.cpp
  )

  add_executable(DeclaraTestsWin32 ${WIN32_TEST_SOURCES})

  target_include_directories(DeclaraTestsWin32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../win32/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
  )

  target_link_libraries(DeclaraTestsWin32 PRIVATE DeclaraWin32Core)

  if(MSVC)
    target_compile_options(DeclaraTestsWin32 PRIVATE /utf-8)
  endif()
endif()

if(APPLE)
  set(MACOS_SOURCES
    src/MacOSMain.mm
  )

  add_executable(DeclaraHelloMacOS ${MACOS_SOURCES})

  target_include_directories(DeclaraHelloMacOS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apple/macos/src/context
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
  )

  target_link_libraries(DeclaraHelloMacOS PRIVATE DeclaraMacOSCore)
  target_compile_options(DeclaraHelloMacOS PRIVATE "-fobjc-arc")
endif()
