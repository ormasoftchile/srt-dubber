cmake_minimum_required(VERSION 3.20)
project(srt-dubber VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Debug: sanitizers
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

# Warnings
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Windows-specific compile flags (UTF-8, NOMINMAX, etc.)
include(cmake/windows-flags.cmake)

# FTXUI
include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG        main
)
set(FTXUI_BUILD_DOCS    OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# nlohmann/json
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# Core-only library (no TUI, no audio) for harness and tests
add_library(srt_dubber_core STATIC
  src/core/recording_flow.cpp
  src/core/assemble_flow.cpp
  src/core/project.cpp
  src/core/resync.cpp
  src/srt/parser.cpp
)
target_include_directories(srt_dubber_core PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/vendor
)
target_link_libraries(srt_dubber_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(srt_dubber_core PUBLIC cxx_std_20)

add_subdirectory(src)

add_executable(srt-dubber main.cpp)
target_link_libraries(srt-dubber PRIVATE srt_dubber_lib)
target_include_directories(srt-dubber PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/vendor
)

# Turn-based harness
add_executable(recording-harness tools/recording_harness.cpp)
target_link_libraries(recording-harness PRIVATE srt_dubber_core)
target_include_directories(recording-harness PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/vendor)

# Recording flow tests
add_executable(recording-flow-tests tests/recording_flow_test.cpp)
target_link_libraries(recording-flow-tests PRIVATE srt_dubber_core)
target_include_directories(recording-flow-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Recording render state tests
add_executable(recording-render-state-tests tests/recording_render_state_test.cpp)
target_link_libraries(recording-render-state-tests PRIVATE srt_dubber_core)
target_include_directories(recording-render-state-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Effect dispatcher integration tests (fakes in tests/audio/ replace real audio headers)
add_executable(effect-dispatcher-tests
  tests/effect_dispatcher_test.cpp
  src/core/effect_dispatcher.cpp
)
target_link_libraries(effect-dispatcher-tests PRIVATE srt_dubber_core)
target_include_directories(effect-dispatcher-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/tests   # stubs (tests/audio/) shadow src/audio/
  ${CMAKE_SOURCE_DIR}/src
)

# Assemble flow tests
add_executable(assemble-flow-tests tests/assemble_flow_test.cpp)
target_link_libraries(assemble-flow-tests PRIVATE srt_dubber_core)
target_include_directories(assemble-flow-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
