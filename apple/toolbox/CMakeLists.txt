cmake_minimum_required(VERSION 3.15)

# Toolbox builds are expected to use a separate toolchain (e.g. Retro68).
add_library(DeclaraAppleToolbox INTERFACE)

target_link_libraries(DeclaraAppleToolbox INTERFACE
  DeclaraAppleCommonCarbon
)

target_include_directories(DeclaraAppleToolbox INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
