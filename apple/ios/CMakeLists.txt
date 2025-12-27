cmake_minimum_required(VERSION 3.15)

add_library(DeclaraAppleiOSUIKit INTERFACE)

target_link_libraries(DeclaraAppleiOSUIKit INTERFACE
  DeclaraAppleCommonCocoa
  "-framework UIKit"
  "-framework Foundation"
)

target_include_directories(DeclaraAppleiOSUIKit INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
