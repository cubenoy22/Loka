cmake_minimum_required(VERSION 3.15)
project(Declara LANGUAGES CXX)

option(DECLARA_ENABLE_APPLE "Enable Apple platform targets" ON)

if(WIN32)
  add_subdirectory(win32)
endif()

if(APPLE AND DECLARA_ENABLE_APPLE)
  add_subdirectory(apple)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
  add_subdirectory(apple/common/carbon)
  add_subdirectory(apple/toolbox)
endif()

if(WIN32 OR (APPLE AND DECLARA_ENABLE_APPLE) OR (CMAKE_SYSTEM_NAME STREQUAL "Retro68"))
  add_subdirectory(example/HelloWorld)
endif()
