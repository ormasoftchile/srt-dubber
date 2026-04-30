#pragma once
// Fake AudioPlayer for the effect_dispatcher_test target.
// Matches the interface of src/audio/player.hpp without miniaudio.
#include <filesystem>
#include <atomic>

class AudioPlayer {
public:
    AudioPlayer() = default;
    ~AudioPlayer() = default;

    bool play(const std::filesystem::path& wav_path) {
        last_play_path = wav_path;
        m_playing = true;
        play_call_count++;
        return true;
    }

    bool stop() {
        m_playing = false;
        stop_call_count++;
        return true;
    }

    bool is_playing() const { return m_playing; }

    // Observability for tests
    std::filesystem::path last_play_path;
    int play_call_count = 0;
    int stop_call_count = 0;

private:
    std::atomic<bool> m_playing {false};
};
