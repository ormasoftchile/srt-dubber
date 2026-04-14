#pragma once
#include <filesystem>
#include <atomic>

struct ma_device;

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    // Begin playback of a WAV file. Returns false on error.
    bool play(const std::filesystem::path& wav_path);

    // Stop playback immediately.
    bool stop();

    bool is_playing() const;

private:
    ma_device* m_device {nullptr};
    std::atomic<bool> m_playing {false};

    static void data_callback(ma_device*, void*, const void*, unsigned int);
};
