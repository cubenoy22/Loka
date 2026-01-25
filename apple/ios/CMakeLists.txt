cmake_minimum_required(VERSION 3.15)

add_library(LokaAppleiOSUIKit INTERFACE)

target_link_libraries(LokaAppleiOSUIKit INTERFACE
  LokaAppleCommonCocoa
  "-framework UIKit"
  "-framework Foundation"
)

target_include_directories(LokaAppleiOSUIKit INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
