cmake_minimum_required(VERSION 3.15)

add_library(LokaAppleCarbon INTERFACE)

target_link_libraries(LokaAppleCarbon INTERFACE
  LokaAppleCommonCarbon
  "-framework Carbon"
)

target_include_directories(LokaAppleCarbon INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
