cmake_minimum_required(VERSION 3.15)

add_library(DeclaraAppleCommonCocoa INTERFACE)

target_include_directories(DeclaraAppleCommonCocoa INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  ${PROJECT_SOURCE_DIR}/common
)
