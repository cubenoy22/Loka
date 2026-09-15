if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "INPUT and OUTPUT are required")
endif()

file(READ "${INPUT}" main_js)
string(REPLACE "\\" "\\\\" main_js "${main_js}")
string(REPLACE "\"" "\\\"" main_js "${main_js}")
string(REPLACE "\n" "\\n\"\n\"" main_js "${main_js}")
file(WRITE "${OUTPUT}" "// Generated from MAIN.JS; do not edit.\nreturn \"${main_js}\";\n")
