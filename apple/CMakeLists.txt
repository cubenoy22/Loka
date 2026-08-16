cmake_minimum_required(VERSION 3.15)

# Enable Objective-C++ when building Apple targets.
if(APPLE)
  enable_language(OBJCXX)
  if(LOKA_MACOS)
    loka_suppress_arclite_link()
  endif()
endif()

add_subdirectory(common/cocoa)

if(IOS)
  add_subdirectory(ios)
  return()
endif()

if(NOT LOKA_MACOS)
  message(FATAL_ERROR
    "Loka's Apple projection currently supports macOS and iOS only; "
    "CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
endif()

add_subdirectory(common/carbon)
add_subdirectory(macos)
add_subdirectory(carbon)
if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  add_subdirectory(toolbox)
endif()
