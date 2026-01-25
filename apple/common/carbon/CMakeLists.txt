cmake_minimum_required(VERSION 3.15)

add_library(LokaAppleCommonCarbon INTERFACE)

target_include_directories(LokaAppleCommonCarbon INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  ${PROJECT_SOURCE_DIR}/common
)
