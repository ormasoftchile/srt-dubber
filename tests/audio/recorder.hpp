#pragma once
// Fake AudioRecorder for the effect_dispatcher_test target.
// Matches the interface of src/audio/recorder.hpp without miniaudio.
#include <filesystem>
#include <atomic>

class AudioRecorder {
public:
    explicit AudioRecorder(int /*device_index*/ = -1) {}
    ~AudioRecorder() = default;

    bool start(const std::filesystem::path& output_wav) {
        last_start_path = output_wav;
        m_recording = true;
        m_warmed_up = false;
        start_call_count++;
        return true;
    }

    bool stop() {
        m_recording = false;
        m_warmed_up = false;
        m_capture_active = false;
        stop_call_count++;
        return true;
    }

    void set_capture_active(bool active) {
        m_capture_active = active;
        capture_active_call_count++;
    }

    bool    is_recording()  const { return m_recording; }
    bool    is_warming_up() const { return !m_warmed_up && m_recording; }
    int64_t elapsed_ms()    const { return 0; }

    // Observability for tests
    std::filesystem::path last_start_path;
    int start_call_count          = 0;
    int stop_call_count           = 0;
    int capture_active_call_count = 0;

private:
    std::atomic<bool> m_recording        {false};
    std::atomic<bool> m_capture_active   {false};
    std::atomic<bool> m_warmed_up        {false};
};
