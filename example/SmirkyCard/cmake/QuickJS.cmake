# Fetch only the engine sources; the upstream project also builds CLI tools.
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(smirkycard_quickjs
  URL https://codeload.github.com/quickjs-ng/quickjs/tar.gz/1ab8676f4b6d6d669baeb5f21790fb9734636a20
  URL_HASH SHA256=c788fe4f65c95ecfa4055c8778e7cb221f68fcc3315686627b0856da5c38514e
  SOURCE_SUBDIR loka-embedded-engine-only
)
FetchContent_MakeAvailable(smirkycard_quickjs)

# QuickJS-ng v0.16.2. Keep the dependency's C dialect and platform flags local.
add_library(smirkycard_quickjs STATIC
  ${smirkycard_quickjs_SOURCE_DIR}/quickjs.c
  ${smirkycard_quickjs_SOURCE_DIR}/dtoa.c
  ${smirkycard_quickjs_SOURCE_DIR}/libregexp.c
  ${smirkycard_quickjs_SOURCE_DIR}/libunicode.c
)
set_target_properties(smirkycard_quickjs PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
target_include_directories(smirkycard_quickjs SYSTEM PUBLIC ${smirkycard_quickjs_SOURCE_DIR})
# The experiment executes synchronously on the main thread; no Atomics/workers.
target_compile_definitions(smirkycard_quickjs PRIVATE _GNU_SOURCE __STDC_NO_ATOMICS__=1)
if(MSVC)
  target_compile_options(smirkycard_quickjs PRIVATE /utf-8)
  target_compile_definitions(smirkycard_quickjs PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_link_libraries(smirkycard_quickjs PRIVATE m)
endif()
