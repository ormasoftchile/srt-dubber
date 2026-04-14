#pragma once
#include <filesystem>
#include <atomic>
#include <cstdint>

// Forward-declare ma types to avoid pulling the heavy header into every TU.
struct ma_device;
struct ma_encoder;

class AudioRecorder {
public:
    explicit AudioRecorder(int device_index = -1);
    ~AudioRecorder();

    // Begin capturing audio and writing to output_wav (PCM 16-bit, mono, 44100 Hz).
    bool start(const std::filesystem::path& output_wav);

    // Stop capturing and flush the WAV file. Safe to call from any thread.
    bool stop();

    bool    is_recording() const;
    int64_t elapsed_ms()   const;

    // Print available capture devices to stderr and return.
    static void list_devices();

private:
    // Samples to discard after device start to let the device (and Bluetooth
    // profile switch) settle before writing audio. 400 ms at 44100 Hz.
    static constexpr unsigned int kWarmupSamples = 44100 * 4 / 10;

    int         m_device_index {-1};

    // miniaudio objects stored on the heap to keep the header include-free.
    ma_device*  m_device  {nullptr};
    ma_encoder* m_encoder {nullptr};

    std::atomic<bool>     m_recording              {false};
    std::atomic<int64_t>  m_start_epoch_ms          {0};
    std::atomic<bool>     m_warmed_up               {false};
    std::atomic<uint64_t> m_warmup_samples_discarded {0};

    // Called by miniaudio on each captured chunk.
    static void data_callback(ma_device*, void*, const void*, unsigned int);
};
