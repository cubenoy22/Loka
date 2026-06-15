cmake_minimum_required(VERSION 3.15)

# Enable Objective-C++ when building Apple targets.
if(APPLE)
  enable_language(OBJCXX)
  if(LOKA_MACOS_SUPPRESS_ARCLITE_LINK)
    foreach(_loka_implicit_link_var
        CMAKE_CXX_IMPLICIT_LINK_LIBRARIES
        CMAKE_OBJCXX_IMPLICIT_LINK_LIBRARIES)
      if(DEFINED ${_loka_implicit_link_var})
        list(FILTER ${_loka_implicit_link_var} EXCLUDE REGEX ".*arclite.*")
      endif()
    endforeach()
  endif()
endif()

add_subdirectory(common/cocoa)
add_subdirectory(common/carbon)
add_subdirectory(macos)
add_subdirectory(ios)
add_subdirectory(carbon)
if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC|Carbon)$")
  add_subdirectory(toolbox)
endif()
