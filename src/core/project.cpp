#include "core/project.hpp"

#include <cassert>
#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "srt/parser.hpp"

namespace core {

// ---------------------------------------------------------------------------
// Minimal hand-rolled JSON helpers (flat schema only, no nesting beyond array)
// ---------------------------------------------------------------------------

namespace json_util {

/// Escape a string value for JSON output.
static std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

/// Unescape a JSON string value (handles common escape sequences).
static std::string unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

/// Extract the string value for a given key from a flat JSON object string.
/// Returns empty string if key not found.
static std::string get_string(const std::string& obj, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = obj.find(search);
    if (pos == std::string::npos) return {};
    pos = obj.find(':', pos + search.size());
    if (pos == std::string::npos) return {};
    pos = obj.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    auto end = pos + 1;
    while (end < obj.size()) {
        if (obj[end] == '\\') { end += 2; continue; }
        if (obj[end] == '"')  break;
        ++end;
    }
    return unescape(obj.substr(pos + 1, end - pos - 1));
}

/// Extract a signed integer value for a given key from a flat JSON object string.
/// Returns default_val if key not found or parse fails.
static int64_t get_int(const std::string& obj, const std::string& key, int64_t default_val = 0) {
    std::string search = "\"" + key + "\"";
    auto pos = obj.find(search);
    if (pos == std::string::npos) return default_val;
    pos = obj.find(':', pos + search.size());
    if (pos == std::string::npos) return default_val;
    // Skip whitespace
    ++pos;
    while (pos < obj.size() && std::isspace((unsigned char)obj[pos])) ++pos;
    // Parse digits (possibly negative)
    std::size_t start = pos;
    if (pos < obj.size() && obj[pos] == '-') ++pos;
    while (pos < obj.size() && std::isdigit((unsigned char)obj[pos])) ++pos;
    if (pos == start) return default_val;
    int64_t val = default_val;
    std::from_chars(obj.data() + start, obj.data() + pos, val);
    return val;
}

/// Split the top-level JSON array into individual object strings.
static std::vector<std::string> split_array(const std::string& json) {
    std::vector<std::string> items;
    auto start = json.find('[');
    if (start == std::string::npos) return items;
    ++start;

    int depth = 0;
    std::size_t obj_start = std::string::npos;

    for (std::size_t i = start; i < json.size(); ++i) {
        char c = json[i];
        if (c == '{') {
            if (depth == 0) obj_start = i;
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && obj_start != std::string::npos) {
                items.push_back(json.substr(obj_start, i - obj_start + 1));
                obj_start = std::string::npos;
            }
        } else if (c == '"') {
            // Skip string literals to avoid false brace matching
            ++i;
            while (i < json.size()) {
                if (json[i] == '\\') { ++i; }
                else if (json[i] == '"') break;
                ++i;
            }
        }
    }
    return items;
}

} // namespace json_util

// ---------------------------------------------------------------------------
// Project implementation
// ---------------------------------------------------------------------------

static std::filesystem::path project_file_for(const std::filesystem::path& srt_path) {
    return srt_path.parent_path() / (srt_path.stem().string() + "-project.json");
}

Project Project::load_or_create(const std::filesystem::path& srt_path) {
    Project proj;
    proj.project_file_ = project_file_for(srt_path);

    // Ensure working directories exist regardless of load/create path
    const auto base = srt_path.parent_path();
    std::filesystem::create_directories(base / "takes");
    std::filesystem::create_directories(base / "processed");
    std::filesystem::create_directories(base / "output");

    if (std::filesystem::exists(proj.project_file_)) {
        // Load existing project.json
        std::ifstream f(proj.project_file_);
        if (!f.is_open()) {
            throw std::runtime_error("Cannot open project file: " + proj.project_file_.string());
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        const std::string json = ss.str();

        for (const auto& obj : json_util::split_array(json)) {
            ProjectEntry e;
            e.index                 = static_cast<int>(json_util::get_int(obj, "index"));
            e.start_ms              = json_util::get_int(obj, "start_ms");
            e.end_ms                = json_util::get_int(obj, "end_ms");
            e.slot_duration_ms      = json_util::get_int(obj, "slot_duration_ms");
            e.text                  = json_util::get_string(obj, "text");
            e.raw_take_path         = json_util::get_string(obj, "raw_take_path");
            e.processed_take_path   = json_util::get_string(obj, "processed_take_path");
            e.raw_duration_ms       = json_util::get_int(obj, "raw_duration_ms", -1);
            e.processed_duration_ms = json_util::get_int(obj, "processed_duration_ms", -1);
            e.status                = take_status_from_string(json_util::get_string(obj, "status"));
            proj.entries_.push_back(std::move(e));
        }
    } else {
        // Create from SRT
        const auto srt_entries = srt::SrtParser::parse(srt_path);
        proj.entries_.reserve(srt_entries.size());

        for (const auto& s : srt_entries) {
            ProjectEntry e;
            e.index            = s.index;
            e.start_ms         = s.start_ms;
            e.end_ms           = s.end_ms;
            e.slot_duration_ms = s.slot_duration_ms;
            e.text             = s.text;
            proj.entries_.push_back(std::move(e));
        }
    }

    // Check if sibling project files exist (e.g. narration-project.json)
    bool has_pending = false;
    for (const auto& e : proj.entries_) {
        if (e.raw_take_path.empty() || e.status == TakeStatus::pending) {
            has_pending = true;
            break;
        }
    }
    if (has_pending) {
        std::error_code ec;
        for (const auto& dir_entry : std::filesystem::directory_iterator(base, ec)) {
            if (!dir_entry.is_regular_file()) continue;
            const auto p = dir_entry.path();
            if (p != proj.project_file_ && p.extension() == ".json" &&
                p.filename().string().ends_with("-project.json")) {
                try {
                    std::ifstream sf(p);
                    if (sf.is_open()) {
                        std::ostringstream ss;
                        ss << sf.rdbuf();
                        const std::string sjson = ss.str();
                        for (const auto& obj : json_util::split_array(sjson)) {
                            int idx = static_cast<int>(json_util::get_int(obj, "index"));
                            std::string raw = json_util::get_string(obj, "raw_take_path");
                            std::string proc = json_util::get_string(obj, "processed_take_path");
                            int64_t proc_dur = json_util::get_int(obj, "processed_duration_ms", -1);
                            int64_t raw_dur = json_util::get_int(obj, "raw_duration_ms", -1);
                            std::string stat_str = json_util::get_string(obj, "status");
                            for (auto& e : proj.entries_) {
                                if (e.index == idx) {
                                    if (e.raw_take_path.empty() && !raw.empty() && std::filesystem::exists(raw)) {
                                        e.raw_take_path = raw;
                                        e.raw_duration_ms = raw_dur;
                                    }
                                    if (e.processed_take_path.empty() && !proc.empty() && std::filesystem::exists(proc)) {
                                        e.processed_take_path = proc;
                                        e.processed_duration_ms = proc_dur;
                                        e.status = take_status_from_string(stat_str);
                                    }
                                }
                            }
                        }
                    }
                } catch (...) {
                    // ignore malformed sibling project file
                }
            }
        }
    }

    // Auto-discover any recorded takes on disk in takes/ and processed/
    const auto takes_dir = base / "takes";
    const auto processed_dir = base / "processed";
    bool any_updated = false;
    for (auto& e : proj.entries_) {
        if (e.raw_take_path.empty() || !std::filesystem::exists(e.raw_take_path)) {
            const auto raw_candidate = takes_dir / (std::to_string(e.index) + ".wav");
            if (std::filesystem::exists(raw_candidate)) {
                e.raw_take_path = raw_candidate.string();
                any_updated = true;
            }
        }
        if (e.processed_take_path.empty() || !std::filesystem::exists(e.processed_take_path)) {
            const auto proc_candidate = processed_dir / (std::to_string(e.index) + ".wav");
            if (std::filesystem::exists(proc_candidate)) {
                e.processed_take_path = proc_candidate.string();
                if (e.status == TakeStatus::pending) {
                    e.status = TakeStatus::ok;
                }
                any_updated = true;
            }
        }
    }

    if (!std::filesystem::exists(proj.project_file_) || any_updated) {
        proj.save();
    }

    return proj;
}

void Project::save() const {
    std::ofstream f(project_file_);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot write project file: " + project_file_.string());
    }

    f << "[\n";
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        f << "  {\n";
        f << "    \"index\": "                 << e.index                              << ",\n";
        f << "    \"start_ms\": "              << e.start_ms                           << ",\n";
        f << "    \"end_ms\": "                << e.end_ms                             << ",\n";
        f << "    \"slot_duration_ms\": "      << e.slot_duration_ms                   << ",\n";
        f << "    \"text\": \""               << json_util::escape(e.text)             << "\",\n";
        f << "    \"raw_take_path\": \""      << json_util::escape(e.raw_take_path)    << "\",\n";
        f << "    \"processed_take_path\": \"" << json_util::escape(e.processed_take_path) << "\",\n";
        f << "    \"raw_duration_ms\": "       << e.raw_duration_ms                    << ",\n";
        f << "    \"processed_duration_ms\": " << e.processed_duration_ms              << ",\n";
        f << "    \"status\": \""              << take_status_to_string(e.status)      << "\"\n";
        f << "  }";
        if (i + 1 < entries_.size()) f << ",";
        f << "\n";
    }
    f << "]\n";
}

std::vector<ProjectEntry>& Project::entries() {
    return entries_;
}

const std::vector<ProjectEntry>& Project::entries() const {
    return entries_;
}

std::filesystem::path Project::takes_dir() const {
    return project_file_.parent_path() / "takes";
}

std::filesystem::path Project::processed_dir() const {
    return project_file_.parent_path() / "processed";
}

std::filesystem::path Project::output_dir() const {
    return project_file_.parent_path() / "output";
}

std::filesystem::path Project::dubbed_video_path() const {
    return output_dir() / (display_name() + "-dubbed.mp4");
}

std::string Project::display_name() const {
    std::string stem = project_file_.stem().string();  // e.g. "episode01-project"
    const std::string suffix = "-project";
    if (stem.size() > suffix.size() &&
        stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0)
        stem.erase(stem.size() - suffix.size());
    return stem;
}

} // namespace core
