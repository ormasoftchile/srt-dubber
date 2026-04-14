if(WIN32)
  # Disable Windows.h min/max macros (common to all Windows compilers)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
  
  # MSVC-specific flags
  if(MSVC)
    # UTF-8 support
    add_compile_options("/utf-8")
    # Static runtime
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()
