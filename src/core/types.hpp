#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace core {

enum class TakeStatus {
    pending,
    ok,
    stretched,
    overflow
};

constexpr std::string_view take_status_to_string(TakeStatus s) {
    switch (s) {
        case TakeStatus::pending:   return "pending";
        case TakeStatus::ok:        return "ok";
        case TakeStatus::stretched: return "stretched";
        case TakeStatus::overflow:  return "overflow";
    }
    std::unreachable();
}

constexpr TakeStatus take_status_from_string(std::string_view s) {
    if (s == "ok")        return TakeStatus::ok;
    if (s == "stretched") return TakeStatus::stretched;
    if (s == "overflow")  return TakeStatus::overflow;
    return TakeStatus::pending;
}

} // namespace core
