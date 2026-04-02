cmake_minimum_required(VERSION 3.15)

# Enable Objective-C++ when building Apple targets.
if(APPLE)
  enable_language(OBJCXX)
endif()

add_subdirectory(common/cocoa)
add_subdirectory(common/carbon)
add_subdirectory(macos)
add_subdirectory(ios)
add_subdirectory(carbon)
if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  add_subdirectory(toolbox)
endif()
