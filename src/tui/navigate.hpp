#pragma once

#include "tui/screen_action.hpp"
#include <functional>

namespace tui {

/// Navigation callback type — used by screen components to signal navigation
/// without exiting the event loop. The payload is optional (e.g., index for
/// review/recording transitions).
using NavigateFunc = std::function<void(ScreenAction action, int payload)>;

} // namespace tui
