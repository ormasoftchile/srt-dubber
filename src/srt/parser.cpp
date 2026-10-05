#include "srt/parser.hpp"

#include <charconv>
#include <cctype>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace srt {

namespace {

// Zero-copy trim leading/trailing whitespace (including \r)
constexpr std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

constexpr bool is_blank(std::string_view line) {
    return trim(line).empty();
}

} // anonymous namespace

// Parse "HH:MM:SS,mmm" or "HH:MM:SS.mmm" → milliseconds
std::expected<int64_t, std::string> SrtParser::timestamp_to_ms(std::string_view ts) {
    if (ts.size() < 12) {
        return std::unexpected(std::format("Invalid SRT timestamp length: {}", ts));
    }

    auto parse_component = [&](std::string_view part) -> std::expected<int64_t, std::string> {
        int64_t val = 0;
        auto [ptr, ec] = std::from_chars(part.data(), part.data() + part.size(), val);
        if (ec != std::errc{} || ptr != part.data() + part.size()) {
            return std::unexpected(std::format("Invalid SRT timestamp component: {}", part));
        }
        return val;
    };

    auto hours   = parse_component(ts.substr(0, 2));
    auto minutes = parse_component(ts.substr(3, 2));
    auto seconds = parse_component(ts.substr(6, 2));
    auto millis  = parse_component(ts.substr(9, 3));

    if (!hours || !minutes || !seconds || !millis) {
        return std::unexpected(std::format("Invalid SRT timestamp components in: {}", ts));
    }

    return (*hours * 3600 + *minutes * 60 + *seconds) * 1000 + *millis;
}

std::vector<SrtEntry> SrtParser::parse(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error(std::format("Cannot open SRT file: {}", path.string()));
    }

    std::vector<SrtEntry> entries;
    std::string line;

    // State machine: each block is [index] [timecode] [text lines...] [blank]
    while (std::getline(file, line)) {
        auto trimmed = trim(line);

        // Skip leading blank lines between entries
        if (trimmed.empty()) {
            continue;
        }

        // 1. Parse index line
        int index = 0;
        auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), index);
        if (ec != std::errc{} || ptr != trimmed.data() + trimmed.size()) {
            // Not a valid index — skip until next blank line
            while (std::getline(file, line) && !is_blank(line)) {}
            continue;
        }

        // 2. Parse timecode line
        if (!std::getline(file, line)) break;
        trimmed = trim(line);

        // Find " --> " separator
        constexpr std::string_view sep = " --> ";
        auto sep_pos = trimmed.find(sep);
        if (sep_pos == std::string_view::npos) {
            // Malformed — skip block
            while (std::getline(file, line) && !is_blank(line)) {}
            continue;
        }

        auto start_res = timestamp_to_ms(trimmed.substr(0, sep_pos));
        std::string_view end_part = trimmed.substr(sep_pos + sep.size());
        // Strip any trailing metadata (e.g. positioning tags)
        auto space_pos = end_part.find(' ');
        if (space_pos != std::string_view::npos) {
            end_part = end_part.substr(0, space_pos);
        }
        auto end_res = timestamp_to_ms(end_part);

        if (!start_res || !end_res) {
            // Malformed timecode — skip block
            while (std::getline(file, line) && !is_blank(line)) {}
            continue;
        }

        int64_t start_ms = *start_res;
        int64_t end_ms = *end_res;

        // 3. Parse text lines until blank line or EOF
        std::string text;
        while (std::getline(file, line)) {
            auto text_line = trim(line);
            if (text_line.empty()) break;
            if (!text.empty()) text += '\n';
            text.append(text_line);
        }

        entries.push_back(SrtEntry{
            .index            = index,
            .start_ms         = start_ms,
            .end_ms           = end_ms,
            .slot_duration_ms = end_ms - start_ms,
            .text             = std::move(text),
        });
    }

    return entries;
}

} // namespace srt
