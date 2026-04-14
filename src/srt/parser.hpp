#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
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

private:
    static int64_t timestamp_to_ms(const std::string& ts);
};

} // namespace srt
