add_library(srt_dubber_lib STATIC
  srt/parser.cpp
  core/project.cpp
  core/resync.cpp
  core/recording_flow.cpp
  core/assemble_flow.cpp
  core/effect_dispatcher.cpp
  core/review_flow.cpp
  core/review_effect_dispatcher.cpp
  core/app_session.cpp
  audio/recorder.cpp
  audio/player.cpp
  ffmpeg/processor.cpp
  ffmpeg/take_pipeline.cpp
  ffmpeg/assembler.cpp
  tui/app.cpp
  tui/screens/session_screen.cpp
  tui/screens/recording_screen.cpp
  tui/screens/review_screen.cpp
  tui/screens/assemble_screen.cpp
)

target_link_libraries(srt_dubber_lib PUBLIC
  ftxui::screen
  ftxui::dom
  ftxui::component
)

target_compile_definitions(srt_dubber_lib PUBLIC
  SRT_DUBBER_VERSION="${PROJECT_VERSION}"
)

target_include_directories(srt_dubber_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/vendor
)

# Platform audio backends
if(APPLE)
  target_link_libraries(srt_dubber_lib PUBLIC
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework CoreFoundation"
  )
elseif(WIN32)
  target_link_libraries(srt_dubber_lib PUBLIC winmm ole32)
elseif(UNIX)
  target_link_libraries(srt_dubber_lib PUBLIC pthread dl m)
endif()
