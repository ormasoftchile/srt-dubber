// player.cpp — AudioPlayer implementation via miniaudio.
// MINIAUDIO_IMPLEMENTATION is already defined in recorder.cpp; do NOT repeat it here.
#include "../../vendor/miniaudio.h"

#include "player.hpp"

#include <cstring>

// ---------------------------------------------------------------------------
// Internal context – carries both the decoder and back-pointer to AudioPlayer.
// Allocated on the heap in play(); freed in stop().
// ---------------------------------------------------------------------------
struct PlayerContext {
    AudioPlayer* player  {nullptr};
    ma_decoder   decoder {};
    ma_device    device  {};
};

// ---------------------------------------------------------------------------
// miniaudio data callback (playback thread)
// ---------------------------------------------------------------------------
void AudioPlayer::data_callback(ma_device*   device,
                                void*        pOutput,
                                const void*  /*pInput*/,
                                unsigned int frameCount)
{
    auto* ctx = reinterpret_cast<PlayerContext*>(device->pUserData);
    if (!ctx || !ctx->player->m_playing.load(std::memory_order_relaxed))
        return;

    ma_uint64 frames_read = 0;
    ma_result result = ma_decoder_read_pcm_frames(
        &ctx->decoder, pOutput, frameCount, &frames_read);

    if (frames_read < frameCount) {
        // Pad remaining output with silence.
        unsigned int bpf = ma_get_bytes_per_frame(
            device->playback.format, device->playback.channels);
        std::memset(
            static_cast<uint8_t*>(pOutput) + frames_read * bpf,
            0,
            (frameCount - static_cast<unsigned int>(frames_read)) * bpf
        );
    }

    if (result != MA_SUCCESS || frames_read < frameCount) {
        // End of file — signal the player to stop.
        ctx->player->m_playing.store(false, std::memory_order_release);
    }
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
AudioPlayer::AudioPlayer()  = default;
AudioPlayer::~AudioPlayer() { stop(); }

// ---------------------------------------------------------------------------
// play()
// ---------------------------------------------------------------------------
bool AudioPlayer::play(const std::filesystem::path& wav_path)
{
    stop(); // stop any active playback first

    auto* ctx    = new PlayerContext{};
    ctx->player  = this;

    // Open decoder (auto-detects sample rate / channels from WAV header,
    // but we request s16 / mono / 44100 for consistency).
    ma_decoder_config dec_cfg = ma_decoder_config_init(
        ma_format_s16, 1, 44100);
    if (ma_decoder_init_file(wav_path.string().c_str(), &dec_cfg, &ctx->decoder)
            != MA_SUCCESS) {
        delete ctx;
        return false;
    }

    // Open playback device backed by the decoder.
    ma_device_config dev_cfg  = ma_device_config_init(ma_device_type_playback);
    dev_cfg.playback.format   = ma_format_s16;
    dev_cfg.playback.channels = 1;
    dev_cfg.sampleRate        = 44100;
    dev_cfg.dataCallback      = AudioPlayer::data_callback;
    dev_cfg.pUserData         = ctx;

    if (ma_device_init(nullptr, &dev_cfg, &ctx->device) != MA_SUCCESS) {
        ma_decoder_uninit(&ctx->decoder);
        delete ctx;
        return false;
    }

    if (ma_device_start(&ctx->device) != MA_SUCCESS) {
        ma_device_uninit(&ctx->device);
        ma_decoder_uninit(&ctx->decoder);
        delete ctx;
        return false;
    }

    // Store context as opaque handle (m_device field reused as void* carrier).
    m_device = reinterpret_cast<ma_device*>(ctx);
    m_playing.store(true, std::memory_order_release);
    return true;
}

// ---------------------------------------------------------------------------
// stop()
// ---------------------------------------------------------------------------
bool AudioPlayer::stop()
{
    if (!m_device)
        return false;

    m_playing.store(false, std::memory_order_release);

    auto* ctx = reinterpret_cast<PlayerContext*>(m_device);
    ma_device_stop(&ctx->device);
    ma_device_uninit(&ctx->device);
    ma_decoder_uninit(&ctx->decoder);
    delete ctx;
    m_device = nullptr;
    return true;
}

// ---------------------------------------------------------------------------
bool AudioPlayer::is_playing() const
{
    return m_playing.load(std::memory_order_acquire);
}
