#pragma once
#include "review_flow.hpp"
#include <optional>

// Forward declaration — keeps srt_dubber_core independent of audio headers.
class AudioPlayer;

namespace core {

class ReviewEffectDispatcher {
public:
    explicit ReviewEffectDispatcher(AudioPlayer& player);

    void apply(const ReviewEffect& effect);
    void apply_all(const std::vector<ReviewEffect>& effects);

    /// Consume a pending navigation effect (returns nullopt if none pending).
    std::optional<int> take_navigate_to_record();
    bool exit_to_session_requested() const;

private:
    AudioPlayer&        player_;
    std::optional<int>  navigate_to_record_;
    bool                exit_requested_ = false;
};

} // namespace core
