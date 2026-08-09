set(_LOKA_WIN32_VISUAL_STYLES_MANIFEST
  "${CMAKE_CURRENT_LIST_DIR}/LokaWin32VisualStyles.manifest")

function(loka_enable_win32_visual_styles target)
  target_sources(${target} PRIVATE
    "${_LOKA_WIN32_VISUAL_STYLES_MANIFEST}"
  )
endfunction()
