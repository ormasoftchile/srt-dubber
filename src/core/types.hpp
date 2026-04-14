#pragma once

#include <string>
#include <stdexcept>

namespace core {

enum class TakeStatus {
    pending,
    ok,
    stretched,
    overflow
};

inline std::string take_status_to_string(TakeStatus s) {
    switch (s) {
        case TakeStatus::pending:   return "pending";
        case TakeStatus::ok:        return "ok";
        case TakeStatus::stretched: return "stretched";
        case TakeStatus::overflow:  return "overflow";
    }
    return "pending";
}

inline TakeStatus take_status_from_string(const std::string& s) {
    if (s == "ok")        return TakeStatus::ok;
    if (s == "stretched") return TakeStatus::stretched;
    if (s == "overflow")  return TakeStatus::overflow;
    return TakeStatus::pending;
}

} // namespace core
