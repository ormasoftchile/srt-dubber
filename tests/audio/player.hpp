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
        m_has_open_resources = true;
        play_call_count++;
        return true;
    }

    bool stop() {
        m_playing = false;
        m_has_open_resources = false;
        stop_call_count++;
        return true;
    }

    bool is_playing() const { return m_playing; }
    bool has_open_resources() const { return m_has_open_resources; }
    void simulate_end_of_file() { m_playing = false; }

    // Observability for tests
    std::filesystem::path last_play_path;
    int play_call_count = 0;
    int stop_call_count = 0;

private:
    std::atomic<bool> m_playing {false};
    bool m_has_open_resources {false};
};
