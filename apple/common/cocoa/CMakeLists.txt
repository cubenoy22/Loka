cmake_minimum_required(VERSION 3.15)

add_library(LokaAppleCommonCocoa INTERFACE)

target_include_directories(LokaAppleCommonCocoa INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  ${PROJECT_SOURCE_DIR}/common
)
