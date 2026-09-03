cmake_minimum_required(VERSION 3.20)
project(srt-dubber VERSION 0.1.0 LANGUAGES CXX)

# Install git hooks so secret scanning runs before every commit.
# Safe to run repeatedly; skipped in CI (no .git dir).
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    COMMAND git config core.hooksPath .githooks
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
  message(STATUS "Git hooks configured (core.hooksPath = .githooks)")
endif()

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
  GIT_TAG        03224dcaf7f9a059dbb13cd2977d3d06729107ed
)
set(FTXUI_BUILD_DOCS    OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# Apply a minimal patch to FTXUI that adds ForceFullClearOnNextDraw().
# This is needed to prevent stray-character artifacts (e.g. a brief 'e' flash)
# on terminals like iTerm2 that briefly render intermediate cursor positions
# during the ResetPosition cursor-up sequence between frames.
# The sentinel file ensures the patch is only applied once per source tree.
FetchContent_GetProperties(ftxui SOURCE_DIR ftxui_SOURCE_DIR)
set(_ftxui_patch_sentinel "${ftxui_SOURCE_DIR}/.srt_dubber_patched")
if(NOT EXISTS "${_ftxui_patch_sentinel}")
  execute_process(
    COMMAND patch -p1 --forward --batch
            -i "${CMAKE_CURRENT_SOURCE_DIR}/patches/ftxui-force-clear.patch"
    WORKING_DIRECTORY "${ftxui_SOURCE_DIR}"
    RESULT_VARIABLE _patch_result
    OUTPUT_VARIABLE _patch_output
    ERROR_VARIABLE  _patch_error
  )
  if(_patch_result EQUAL 0)
    file(TOUCH "${_ftxui_patch_sentinel}")
    message(STATUS "Applied FTXUI patch: ForceFullClearOnNextDraw()")
  else()
    message(WARNING
      "FTXUI patch failed (may already be applied or source changed).\n"
      "${_patch_output}${_patch_error}")
  endif()
endif()

# nlohmann/json
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# miniaudio (single-header audio library, gitignored)
set(_miniaudio_header "${CMAKE_SOURCE_DIR}/vendor/miniaudio.h")
if(NOT EXISTS "${_miniaudio_header}")
  message(STATUS "vendor/miniaudio.h not found; downloading miniaudio 0.11.21...")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/mackron/miniaudio/0.11.21/miniaudio.h"
    "${_miniaudio_header}"
    STATUS _miniaudio_status
    SHOW_PROGRESS
  )
  list(GET _miniaudio_status 0 _miniaudio_code)
  if(NOT _miniaudio_code EQUAL 0)
    file(REMOVE "${_miniaudio_header}")
    message(FATAL_ERROR
      "Failed to download miniaudio.h. Please download it manually:\n"
      "  curl -L https://raw.githubusercontent.com/mackron/miniaudio/0.11.21/miniaudio.h -o vendor/miniaudio.h"
    )
  endif()
endif()

# Core-only library (no TUI, no audio) for harness and tests
add_library(srt_dubber_core STATIC
  src/core/recording_flow.cpp
  src/core/assemble_flow.cpp
  src/core/review_flow.cpp
  src/core/project.cpp
  src/core/resync.cpp
  src/core/app_session.cpp
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

enable_testing()

# Turn-based harness
add_executable(recording-harness tools/recording_harness.cpp)
target_link_libraries(recording-harness PRIVATE srt_dubber_core)
target_include_directories(recording-harness PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/vendor)

# Recording flow tests
add_executable(recording-flow-tests tests/recording_flow_test.cpp)
target_link_libraries(recording-flow-tests PRIVATE srt_dubber_core)
target_include_directories(recording-flow-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME recording-flow-tests COMMAND recording-flow-tests)

# Recording render state tests
add_executable(recording-render-state-tests tests/recording_render_state_test.cpp)
target_link_libraries(recording-render-state-tests PRIVATE srt_dubber_core)
target_include_directories(recording-render-state-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME recording-render-state-tests COMMAND recording-render-state-tests)

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
add_test(NAME effect-dispatcher-tests COMMAND effect-dispatcher-tests)

# Assemble flow tests
add_executable(assemble-flow-tests tests/assemble_flow_test.cpp)
target_link_libraries(assemble-flow-tests PRIVATE srt_dubber_core)
target_include_directories(assemble-flow-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME assemble-flow-tests COMMAND assemble-flow-tests)

# Take processing and mux command tests
add_executable(take-pipeline-tests tests/take_pipeline_test.cpp)
target_link_libraries(take-pipeline-tests PRIVATE srt_dubber_lib)
target_include_directories(take-pipeline-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME take-pipeline-tests COMMAND take-pipeline-tests)

# Review flow tests
add_executable(review-flow-tests tests/review_flow_test.cpp)
target_link_libraries(review-flow-tests PRIVATE srt_dubber_core)
target_include_directories(review-flow-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME review-flow-tests COMMAND review-flow-tests)

# AppSession router state machine tests
add_executable(app-session-tests tests/app_session_test.cpp)
target_link_libraries(app-session-tests PRIVATE srt_dubber_core)
target_include_directories(app-session-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME app-session-tests COMMAND app-session-tests)

# Multi-screen app harness (nav + all screen machines, no audio/TUI)
add_executable(app-harness tools/app_harness.cpp)
target_link_libraries(app-harness PRIVATE srt_dubber_core nlohmann_json::nlohmann_json)
target_include_directories(app-harness PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Integration test: app-harness shell script
configure_file(
  ${CMAKE_SOURCE_DIR}/tests/app_harness_test.sh
  ${CMAKE_BINARY_DIR}/app_harness_test.sh
  @ONLY
  NEWLINE_STYLE UNIX
)
if(WIN32)
  add_test(
    NAME app-harness-integration
    COMMAND bash app_harness_test.sh ./$<CONFIG>/app-harness.exe
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
else()
  add_test(
    NAME app-harness-integration
    COMMAND bash app_harness_test.sh $<TARGET_FILE:app-harness>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endif()
