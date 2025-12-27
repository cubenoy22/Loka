cmake_minimum_required(VERSION 3.15)
project(Declara LANGUAGES CXX)

option(DECLARA_ENABLE_APPLE "Enable Apple platform targets" ON)

if(WIN32)
  add_subdirectory(win32)
  add_subdirectory(example/HelloWorld)
endif()

if(APPLE AND DECLARA_ENABLE_APPLE)
  add_subdirectory(apple)
endif()
