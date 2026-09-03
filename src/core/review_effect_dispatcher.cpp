#include "review_effect_dispatcher.hpp"
#include "audio/player.hpp"
#include <variant>

namespace core {

ReviewEffectDispatcher::ReviewEffectDispatcher(AudioPlayer& player)
    : player_(player) {}

void ReviewEffectDispatcher::apply(const ReviewEffect& effect) {
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, PlayReviewTake>) {
            player_.play(v.path);

        } else if constexpr (std::is_same_v<T, StopReviewPlay>) {
            player_.stop();

        } else if constexpr (std::is_same_v<T, NavigateToRecord>) {
            navigate_to_record_ = v.idx;

        } else if constexpr (std::is_same_v<T, ExitToSession>) {
            exit_requested_ = true;
        }
    }, effect);
}

void ReviewEffectDispatcher::apply_all(const std::vector<ReviewEffect>& effects) {
    for (const auto& e : effects)
        apply(e);
}

std::optional<int> ReviewEffectDispatcher::take_navigate_to_record() {
    auto nav = navigate_to_record_;
    navigate_to_record_.reset();
    return nav;
}

bool ReviewEffectDispatcher::exit_to_session_requested() const {
    return exit_requested_;
}

} // namespace core
