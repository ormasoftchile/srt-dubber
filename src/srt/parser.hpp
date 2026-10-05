#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace srt {

struct SrtEntry {
    int index;
    int64_t start_ms;
    int64_t end_ms;
    int64_t slot_duration_ms;
    std::string text;
};

class SrtParser {
public:
    static std::vector<SrtEntry> parse(const std::filesystem::path& path);
    static std::expected<int64_t, std::string> timestamp_to_ms(std::string_view ts);
};

} // namespace srt
