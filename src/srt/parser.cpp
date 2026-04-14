#include "parser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <algorithm>

namespace srt {

namespace {

// Trim leading/trailing whitespace (including \r)
std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

bool is_blank(const std::string& line) {
    return trim(line).empty();
}

} // anonymous namespace

// Parse "HH:MM:SS,mmm" → milliseconds
int64_t SrtParser::timestamp_to_ms(const std::string& ts) {
    // Expected format: HH:MM:SS,mmm
    if (ts.size() < 12) {
        throw std::runtime_error("Invalid SRT timestamp: " + ts);
    }

    auto safe_int = [&](const std::string& s) -> int64_t {
        try {
            return std::stoll(s);
        } catch (...) {
            throw std::runtime_error("Invalid SRT timestamp component in: " + ts);
        }
    };

    int64_t hours   = safe_int(ts.substr(0, 2));
    int64_t minutes = safe_int(ts.substr(3, 2));
    int64_t seconds = safe_int(ts.substr(6, 2));
    int64_t millis  = safe_int(ts.substr(9, 3));

    return (hours * 3600 + minutes * 60 + seconds) * 1000 + millis;
}

std::vector<SrtEntry> SrtParser::parse(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open SRT file: " + path.string());
    }

    std::vector<SrtEntry> entries;
    std::string line;

    // State machine: each block is  [index] [timecode] [text lines...] [blank]
    while (std::getline(file, line)) {
        line = trim(line);

        // Skip leading blank lines between entries
        if (is_blank(line)) {
            continue;
        }

        // 1. Parse index line
        int index = 0;
        try {
            index = std::stoi(line);
        } catch (...) {
            // Not a valid index — skip until next blank line
            while (std::getline(file, line) && !is_blank(trim(line))) {}
            continue;
        }

        // 2. Parse timecode line
        if (!std::getline(file, line)) break;
        line = trim(line);

        // Find " --> " separator
        const std::string sep = " --> ";
        auto sep_pos = line.find(sep);
        if (sep_pos == std::string::npos) {
            // Malformed — skip block
            while (std::getline(file, line) && !is_blank(trim(line))) {}
            continue;
        }

        int64_t start_ms = 0, end_ms = 0;
        try {
            start_ms = timestamp_to_ms(line.substr(0, sep_pos));
            // Timecode line may have extra metadata after end timestamp; take only first token
            std::string end_part = line.substr(sep_pos + sep.size());
            // Strip any trailing metadata (e.g. positioning tags)
            auto space_pos = end_part.find(' ');
            if (space_pos != std::string::npos) {
                end_part = end_part.substr(0, space_pos);
            }
            end_ms = timestamp_to_ms(end_part);
        } catch (const std::exception& e) {
            // Malformed timecode — skip block
            while (std::getline(file, line) && !is_blank(trim(line))) {}
            continue;
        }

        // 3. Parse text lines until blank line or EOF
        std::string text;
        while (std::getline(file, line)) {
            line = trim(line);
            if (is_blank(line)) break;
            if (!text.empty()) text += '\n';
            text += line;
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
