#pragma once
#include <filesystem>
#include <atomic>
#include <cstdint>

// Forward-declare ma types to avoid pulling the heavy header into every TU.
struct ma_device;
struct ma_encoder;

class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();

    // Begin capturing audio and writing to output_wav (PCM 16-bit, mono, 44100 Hz).
    bool start(const std::filesystem::path& output_wav);

    // Stop capturing and flush the WAV file. Safe to call from any thread.
    bool stop();

    bool    is_recording() const;
    int64_t elapsed_ms()   const;

private:
    // miniaudio objects stored on the heap to keep the header include-free.
    ma_device*  m_device  {nullptr};
    ma_encoder* m_encoder {nullptr};

    std::atomic<bool>    m_recording {false};
    std::atomic<int64_t> m_start_epoch_ms {0};

    // Called by miniaudio on each captured chunk.
    static void data_callback(ma_device*, void*, const void*, unsigned int);
};
