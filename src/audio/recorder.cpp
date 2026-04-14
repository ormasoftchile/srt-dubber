// MINIAUDIO_IMPLEMENTATION must be defined in exactly ONE translation unit.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio/recorder.hpp"

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

    // Discard early samples while the device (and Bluetooth profile) settles.
    if (!self->m_warmed_up.load(std::memory_order_relaxed)) {
        uint64_t discarded = self->m_warmup_samples_discarded.fetch_add(
            frameCount, std::memory_order_relaxed) + frameCount;
        if (discarded >= kWarmupSamples) {
            self->m_warmed_up.store(true, std::memory_order_release);
            // Start the visible timer only once warm-up is complete.
            using namespace std::chrono;
            self->m_start_epoch_ms.store(
                duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()).count(),
                std::memory_order_relaxed);
        }
        return;
    }

    ma_encoder_write_pcm_frames(self->m_encoder, pInput, frameCount, nullptr);
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
AudioRecorder::AudioRecorder(int device_index)
    : m_device_index(device_index)
    , m_device (new ma_device{})
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

    // Reset warm-up state for each new recording.
    m_warmed_up.store(false, std::memory_order_relaxed);
    m_warmup_samples_discarded.store(0, std::memory_order_relaxed);

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

    // If a specific device index was requested, enumerate to find its ID.
    ma_device_id selected_id{};
    if (m_device_index >= 0) {
        ma_context context;
        if (ma_context_init(nullptr, 0, nullptr, &context) == MA_SUCCESS) {
            ma_device_info* capture_devices = nullptr;
            ma_uint32       capture_count   = 0;
            if (ma_context_get_devices(&context, nullptr, nullptr,
                                       &capture_devices, &capture_count) == MA_SUCCESS
                && (ma_uint32)m_device_index < capture_count)
            {
                selected_id = capture_devices[m_device_index].id;
                dev_cfg.capture.pDeviceID = &selected_id;
            } else {
                fprintf(stderr,
                    "[audio] Warning: device index %d not found (%u devices). "
                    "Falling back to default.\n",
                    m_device_index, capture_count);
            }
            ma_context_uninit(&context);
        }
    }

    if (ma_device_init(nullptr, &dev_cfg, m_device) != MA_SUCCESS) {
        ma_encoder_uninit(m_encoder);
        return false;
    }

    fprintf(stderr, "[audio] Recording device: %s\n", m_device->capture.name);

    if (m_device->sampleRate != 44100) {
        fprintf(stderr,
            "[audio] Warning: device sample rate is %u Hz (requested 44100). "
            "Audio quality may be reduced.\n",
            m_device->sampleRate);
    }

    if (ma_device_start(m_device) != MA_SUCCESS) {
        ma_device_uninit(m_device);
        ma_encoder_uninit(m_encoder);
        return false;
    }

    // m_start_epoch_ms is set by the data callback once warm-up completes.
    // Initialise to "now" as a safe fallback (elapsed_ms won't be called
    // before warm-up in normal usage, but guard against it).
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

bool AudioRecorder::is_warming_up() const
{
    return !m_warmed_up.load(std::memory_order_relaxed);
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

// ---------------------------------------------------------------------------
// list_devices()
// ---------------------------------------------------------------------------
void AudioRecorder::list_devices()
{
    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
        fprintf(stderr, "[audio] Failed to initialise audio context.\n");
        return;
    }

    ma_device_info* capture_devices   = nullptr;
    ma_uint32       capture_count     = 0;
    ma_device_info* playback_devices  = nullptr;
    ma_uint32       playback_count    = 0;

    if (ma_context_get_devices(&context,
                               &playback_devices, &playback_count,
                               &capture_devices,  &capture_count) != MA_SUCCESS)
    {
        fprintf(stderr, "[audio] Failed to enumerate devices.\n");
        ma_context_uninit(&context);
        return;
    }

    fprintf(stderr, "[audio] Capture devices (%u):\n", capture_count);
    for (ma_uint32 i = 0; i < capture_count; ++i) {
        fprintf(stderr, "  [%u] %s%s\n",
                i,
                capture_devices[i].name,
                capture_devices[i].isDefault ? "  (default)" : "");
    }

    ma_context_uninit(&context);
}
