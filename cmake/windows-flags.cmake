if(WIN32)
  # UTF-8 support
  add_compile_options("/utf-8")
  # Disable Windows.h min/max macros
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
  # Static runtime
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
