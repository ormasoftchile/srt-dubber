// MINIAUDIO_IMPLEMENTATION must be defined in exactly ONE translation unit.
#define MINIAUDIO_IMPLEMENTATION
#include "../../vendor/miniaudio.h"

#include "recorder.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

// ---------------------------------------------------------------------------
// miniaudio data callback – called from the audio thread.
// ---------------------------------------------------------------------------
void AudioRecorder::data_callback(ma_device* device,
                                  void*       /*pOutput*/,
                                  const void* pInput,
                                  unsigned int frameCount)
{
    auto* self = reinterpret_cast<AudioRecorder*>(device->pUserData);
    if (!self || !self->m_recording.load(std::memory_order_relaxed))
        return;
    if (!pInput || frameCount == 0)
        return;

    ma_encoder_write_pcm_frames(self->m_encoder, pInput, frameCount, nullptr);
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
AudioRecorder::AudioRecorder()
    : m_device (new ma_device{})
    , m_encoder(new ma_encoder{})
{}

AudioRecorder::~AudioRecorder()
{
    if (m_recording.load())
        stop();

    delete m_device;
    delete m_encoder;
}

// ---------------------------------------------------------------------------
// start()
// ---------------------------------------------------------------------------
bool AudioRecorder::start(const std::filesystem::path& output_wav)
{
    if (m_recording.load())
        return false;

    // --- encoder ---
    ma_encoder_config enc_cfg = ma_encoder_config_init(
        ma_encoding_format_wav,
        ma_format_s16,
        /*channels=*/1,
        /*sampleRate=*/44100
    );

    if (ma_encoder_init_file(output_wav.string().c_str(), &enc_cfg, m_encoder)
            != MA_SUCCESS)
        return false;

    // --- capture device ---
    ma_device_config dev_cfg = ma_device_config_init(ma_device_type_capture);
    dev_cfg.capture.format   = ma_format_s16;
    dev_cfg.capture.channels = 1;
    dev_cfg.sampleRate        = 44100;
    dev_cfg.dataCallback      = AudioRecorder::data_callback;
    dev_cfg.pUserData         = this;

    if (ma_device_init(nullptr, &dev_cfg, m_device) != MA_SUCCESS) {
        ma_encoder_uninit(m_encoder);
        return false;
    }

    if (ma_device_start(m_device) != MA_SUCCESS) {
        ma_device_uninit(m_device);
        ma_encoder_uninit(m_encoder);
        return false;
    }

    using namespace std::chrono;
    m_start_epoch_ms.store(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
    );
    m_recording.store(true, std::memory_order_release);
    return true;
}

// ---------------------------------------------------------------------------
// stop()
// ---------------------------------------------------------------------------
bool AudioRecorder::stop()
{
    if (!m_recording.exchange(false))
        return false;

    ma_device_stop(m_device);
    ma_device_uninit(m_device);
    ma_encoder_uninit(m_encoder);

    // Reset objects so they can be reused.
    *m_device  = ma_device{};
    *m_encoder = ma_encoder{};

    return true;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
bool AudioRecorder::is_recording() const
{
    return m_recording.load(std::memory_order_acquire);
}

int64_t AudioRecorder::elapsed_ms() const
{
    if (!is_recording())
        return 0;

    using namespace std::chrono;
    int64_t now = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    return now - m_start_epoch_ms.load(std::memory_order_relaxed);
}
