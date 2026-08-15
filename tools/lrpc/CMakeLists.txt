cmake_minimum_required(VERSION 3.15)
project(Lrpc LANGUAGES CXX)

# lrpc is a host tool and is never part of a target build. #185's cross
# configure ruling is that this is configured natively once, and that a cross
# configure takes the built artifact through LOKA_LRPC rather than trying to
# bootstrap it -- which is what keeps scripts/macos/build-10_4.sh, running
# under a C++98-only gcc-4.0, able to take part by consuming artifacts only.
#
# Nothing here is added to the target builds: no platform CMakeLists includes
# this directory. Point a native configure at it:
#
#   cmake -S tools/lrpc -B build/host/lrpc
#   cmake --build build/host/lrpc

add_executable(lrpc
  main.cpp
  HostFile.cpp
  LrpkStager.cpp
  PackManifest.cpp
  LrpkWriter.cpp
  Utf8Path.cpp
)

# tools/ is the include root: LrpkWriter.cpp already spells its own header as
# "lrpc/LrpkWriter.hpp", so the tool keeps that root rather than acquiring a
# second one.
target_include_directories(lrpc PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../common
)

if(COMMAND loka_enable_warnings)
  loka_enable_warnings(lrpc)
endif()

set_target_properties(lrpc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
