#pragma once
#include "ftxui/dom/elements.hpp"

#ifndef SRT_DUBBER_VERSION
#define SRT_DUBBER_VERSION "0.0.0-dev"
#endif

namespace tui {

using namespace ftxui;

/// Permanent app identity header shown in all screens.
/// context: right-aligned annotation, e.g. "Caption 3/42", "Review"; empty = none
inline Element app_header(const std::string& context = "") {
    Element right;
    if (context.empty()) {
        right = dim(text(std::string("v") + SRT_DUBBER_VERSION + "  "));
    } else {
        right = dim(text(context + "  \xc2\xb7  v" + SRT_DUBBER_VERSION + "  "));
    }
    return hbox({
        text("  "),
        bold(text("\xe2\x97\x89  srt-dubber")),
        filler(),
        right,
    });
}

/// Live recording header — replaces app identity during active recording.
/// warming_up: show yellow "warming up…" instead of red "recording…"
inline Element app_header_recording(bool warming_up = false) {
    Element status = warming_up
        ? color(Color::Yellow, bold(text("\xe2\x97\x8f  warming up\xe2\x80\xa6")))
        : color(Color::Red,    bold(text("\xe2\x97\x8f  recording\xe2\x80\xa6")));
    return hbox({
        text("  "),
        status,
        filler(),
        dim(text(std::string("v") + SRT_DUBBER_VERSION + "  ")),
    });
}

} // namespace tui
