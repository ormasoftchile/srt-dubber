#pragma once
#include "ftxui/dom/elements.hpp"

namespace tui {

using namespace ftxui;

/// Permanent app identity header shown in all screens.
/// context: right-aligned annotation, e.g. "Caption 3/42", "Review"; empty = none
inline Element app_header(const std::string& context = "") {
    if (context.empty()) {
        return hbox({
            text("  "),
            bold(text("\xe2\x97\x89  srt-dubber")),
            filler(),
        });
    }
    return hbox({
        text("  "),
        bold(text("\xe2\x97\x89  srt-dubber")),
        filler(),
        dim(text(context + "  ")),
    });
}

/// Live recording header — replaces app identity during active recording.
/// warming_up: show yellow "warming up…" instead of red "recording…"
inline Element app_header_recording(bool warming_up = false) {
    if (warming_up) {
        return hbox({
            text("  "),
            color(Color::Yellow, bold(text("\xe2\x97\x8f  warming up\xe2\x80\xa6"))),
            filler(),
        });
    }
    return hbox({
        text("  "),
        color(Color::Red, bold(text("\xe2\x97\x8f  recording\xe2\x80\xa6"))),
        filler(),
    });
}

} // namespace tui
