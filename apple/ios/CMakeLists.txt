cmake_minimum_required(VERSION 3.15)

if(CMAKE_OSX_DEPLOYMENT_TARGET AND
   CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 13.0)
  message(FATAL_ERROR
    "LokaUIKitHelloWorld uses the modern UIScene lifecycle and requires iOS 13 or newer; "
    "use the separate legacy Apple profile for older systems")
endif()

add_library(LokaAppleiOSUIKit INTERFACE)

target_link_libraries(LokaAppleiOSUIKit INTERFACE
  LokaAppleCommonCocoa
  "-framework UIKit"
  "-framework Foundation"
)

target_include_directories(LokaAppleiOSUIKit INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

add_executable(LokaUIKitHelloWorld MACOSX_BUNDLE
  src/UIKitHelloWorld.mm
)
loka_enable_warnings(LokaUIKitHelloWorld)

target_link_libraries(LokaUIKitHelloWorld PRIVATE
  LokaAppleiOSUIKit
)

target_compile_options(LokaUIKitHelloWorld PRIVATE
  -fno-exceptions
  -fno-rtti
  -fno-objc-arc
)

set_target_properties(LokaUIKitHelloWorld PROPERTIES
  CXX_STANDARD 98
  CXX_STANDARD_REQUIRED YES
  OBJCXX_STANDARD 98
  OBJCXX_STANDARD_REQUIRED YES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/Info.plist.in"
  MACOSX_BUNDLE_GUI_IDENTIFIER "io.lo-ka.loka.uikit-hello-world"
  MACOSX_BUNDLE_BUNDLE_NAME "Loka UIKit Hello World"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  MACOSX_BUNDLE_BUNDLE_VERSION "1"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "io.lo-ka.loka.uikit-hello-world"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
)
