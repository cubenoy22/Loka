cmake_minimum_required(VERSION 3.15)

add_library(DeclaraAppleCarbon INTERFACE)

target_link_libraries(DeclaraAppleCarbon INTERFACE
  DeclaraAppleCommonCarbon
  "-framework Carbon"
)

target_include_directories(DeclaraAppleCarbon INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
