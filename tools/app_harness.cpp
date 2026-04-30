/// tools/app_harness.cpp
/// Turn-based JSON harness for the full AppSession state machine.
/// Drives nav_step + all screen machines via JSON stdin/stdout.
/// No audio, no TUI — srt_dubber_core only.

#include "core/app_session.hpp"
#include "core/recording_flow.hpp"
#include "core/recording_render_state.hpp"
#include "core/recording_effects.hpp"
#include "core/review_flow.hpp"
#include "core/assemble_flow.hpp"
#include "core/project.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using json = nlohmann::json;
using namespace core;

// ─── Fixture ──────────────────────────────────────────────────────────────

static std::vector<ProjectEntry> create_fixture() {
    std::vector<ProjectEntry> entries;
    entries.push_back({1, 1000,  3000, 2000, "Hello world",  "", "", -1, -1, TakeStatus::pending});
    entries.push_back({2, 3500,  5500, 2000, "How are you?", "", "", -1, -1, TakeStatus::pending});
    entries.push_back({3, 6000,  8000, 2000, "Goodbye!",     "", "", -1, -1, TakeStatus::pending});
    return entries;
}

// ─── Parse helpers ────────────────────────────────────────────────────────

static std::optional<NavCmd> parse_nav_cmd(const std::string& s) {
    if (s == "GoRecord")        return NavCmd::GoRecord;
    if (s == "GoReview")        return NavCmd::GoReview;
    if (s == "GoAssemble")      return NavCmd::GoAssemble;
    if (s == "Quit")            return NavCmd::Quit;
    if (s == "RecordingDone")   return NavCmd::RecordingDone;
    if (s == "ReviewGoRecord")  return NavCmd::ReviewGoRecord;
    if (s == "ReviewGoSession") return NavCmd::ReviewGoSession;
    if (s == "AssembleDone")    return NavCmd::AssembleDone;
    return std::nullopt;
}

static std::optional<AssembleCmd> parse_assemble_cmd_str(const std::string& s) {
    if (s == "Start")           return AssembleCmd::Start;
    if (s == "Back")            return AssembleCmd::Back;
    if (s == "AssemblyDone")    return AssembleCmd::AssemblyDone;
    if (s == "AssemblyFailed")  return AssembleCmd::AssemblyFailed;
    return std::nullopt;
}

// ─── Screen name ──────────────────────────────────────────────────────────

static std::string screen_name(AppScreen s) {
    switch (s) {
        case AppScreen::Session:   return "session";
        case AppScreen::Recording: return "recording";
        case AppScreen::Review:    return "review";
        case AppScreen::Assemble:  return "assemble";
    }
    return "unknown";
}

// ─── Serialisation helpers ────────────────────────────────────────────────

static json nav_to_json(const NavState& s) {
    return {
        {"screen",        screen_name(s.screen)},
        {"recording_idx", s.recording_idx},
        {"review_idx",    s.review_idx},
        {"running",       s.running}
    };
}

static json recording_render_to_json(const RecordingRenderState& rs) {
    return {
        {"current_idx",      rs.current_idx},
        {"total",            rs.total},
        {"phase",            rs.phase_label},
        {"has_take",         rs.has_take},
        {"entry_text",       rs.entry_text},
        {"slot_duration_ms", rs.slot_duration_ms},
        {"elapsed_ms",       rs.elapsed_ms}
    };
}

static json review_render_to_json(const ReviewRenderState& rs) {
    json rows = json::array();
    for (const auto& row : rs.rows) {
        rows.push_back({
            {"idx",          row.idx},
            {"text_preview", row.text_preview},
            {"slot_ms",      row.slot_ms},
            {"proc_ms",      row.proc_ms},
            {"status_label", row.status_label},
            {"has_take",     row.has_take}
        });
    }
    return {
        {"selected_idx", rs.selected_idx},
        {"total",        rs.total},
        {"rows",         rows}
    };
}

static json assemble_render_to_json(const AssembleRenderState& rs) {
    return {
        {"phase_label", rs.phase_label},
        {"log_lines",   rs.log_lines},
        {"footer",      rs.footer},
        {"allow_back",  rs.allow_back}
    };
}

// ─── Effect name helpers ──────────────────────────────────────────────────

static std::string recording_effect_name(const RecordingEffect& eff) {
    return std::visit([](const auto& e) -> std::string {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, StartCountdown>)    return "StartCountdown";
        if constexpr (std::is_same_v<T, CancelCountdown>)   return "CancelCountdown";
        if constexpr (std::is_same_v<T, ActivateCapture>)   return "ActivateCapture";
        if constexpr (std::is_same_v<T, StopRecording>)     return "StopRecording";
        if constexpr (std::is_same_v<T, PlayTake>)          return "PlayTake";
        if constexpr (std::is_same_v<T, StopPlayback>)      return "StopPlayback";
        if constexpr (std::is_same_v<T, SaveTake>)          return "SaveTake";
        if constexpr (std::is_same_v<T, ClearTake>)         return "ClearTake";
        if constexpr (std::is_same_v<T, SaveProject>)       return "SaveProject";
        if constexpr (std::is_same_v<T, ExitToSession>)     return "ExitToSession";
        return "Unknown";
    }, eff);
}

static std::string review_effect_name(const ReviewEffect& eff) {
    return std::visit([](const auto& e) -> std::string {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, PlayReviewTake>)     return "PlayReviewTake";
        if constexpr (std::is_same_v<T, StopReviewPlay>)     return "StopReviewPlay";
        if constexpr (std::is_same_v<T, NavigateToRecord>)   return "NavigateToRecord";
        if constexpr (std::is_same_v<T, ExitToSession>)      return "ExitToSession";
        return "Unknown";
    }, eff);
}

static std::string assemble_effect_name(const AssembleEffect& eff) {
    return std::visit([](const auto& e) -> std::string {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, SpawnAssembly>)  return "SpawnAssembly";
        if constexpr (std::is_same_v<T, AppendLog>)      return "AppendLog";
        if constexpr (std::is_same_v<T, SaveProject>)    return "SaveProject";
        if constexpr (std::is_same_v<T, ExitToSession>)  return "ExitToSession";
        return "Unknown";
    }, eff);
}

// ─── Build screen_state JSON for current nav screen ───────────────────────

static json screen_state_json(AppScreen                  screen,
                               const FlowState&           flow,
                               const ReviewState&         review,
                               const AssembleState&       assemble,
                               const std::vector<ProjectEntry>& entries) {
    switch (screen) {
        case AppScreen::Session:
            return {{"type", "session"}};

        case AppScreen::Recording: {
            int idx = std::clamp(flow.current_idx, 0, (int)entries.size() - 1);
            const auto& e = entries[idx];
            auto rs = render_state(flow, e.text, e.slot_duration_ms, 0);
            return recording_render_to_json(rs);
        }

        case AppScreen::Review: {
            auto rs = review_render_state(review, entries);
            return review_render_to_json(rs);
        }

        case AppScreen::Assemble: {
            auto rs = assemble_render_state(assemble);
            return assemble_render_to_json(rs);
        }
    }
    return {};
}

// ─── Emit a full output frame ─────────────────────────────────────────────

static void emit(const NavState&                  nav,
                 const FlowState&                 flow,
                 const ReviewState&               review,
                 const AssembleState&             assemble,
                 const std::vector<ProjectEntry>& entries,
                 const json&                      effects,
                 const std::string&               error = "",
                 bool                             exit_flag = false) {
    json out;
    out["nav"]          = nav_to_json(nav);
    out["screen_state"] = screen_state_json(nav.screen, flow, review, assemble, entries);
    out["effects"]      = effects;
    out["error"]        = error.empty() ? json(nullptr) : json(error);
    if (exit_flag) out["exit"] = true;
    std::cout << out.dump(2) << "\n";
}

// ─── Main ─────────────────────────────────────────────────────────────────

int main() {
    auto entries = create_fixture();

    NavState nav;

    FlowState flow;
    flow.current_idx = 0;
    flow.total       = static_cast<int>(entries.size());
    flow.phase       = FlowPhase::Idle;
    flow.has_take    = false;

    ReviewState review;
    review.selected_idx = 0;
    review.total        = static_cast<int>(entries.size());

    AssembleState assemble;

    // Emit initial state
    emit(nav, flow, review, assemble, entries, json::array());

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        json in;
        try {
            in = json::parse(line);
        } catch (const json::exception& e) {
            json err_out;
            err_out["nav"]          = nav_to_json(nav);
            err_out["screen_state"] = screen_state_json(nav.screen, flow, review, assemble, entries);
            err_out["effects"]      = json::array();
            err_out["error"]        = std::string("JSON parse error: ") + e.what();
            std::cout << err_out.dump(2) << "\n";
            continue;
        }

        std::string screen_target = in.value("screen", "");
        std::string input         = in.value("input", "");

        // ── NAV layer ────────────────────────────────────────────────────
        if (screen_target == "nav") {
            auto cmd = parse_nav_cmd(input);
            if (!cmd) {
                emit(nav, flow, review, assemble, entries, json::array(),
                     "unknown command: " + input);
                continue;
            }

            // For ReviewGoRecord, pass current review selection as payload
            int payload = 0;
            if (*cmd == NavCmd::ReviewGoRecord)
                payload = review.selected_idx;

            auto t = nav_step(nav, *cmd, payload);
            nav = t.next;

            // Sync sub-screen states when switching screens
            if (nav.screen == AppScreen::Recording) {
                flow.current_idx = nav.recording_idx;
                flow.phase       = FlowPhase::Idle;
                flow.has_take    = !entries[flow.current_idx].raw_take_path.empty();
            }
            if (nav.screen == AppScreen::Review) {
                review.selected_idx = nav.review_idx;
            }
            if (nav.screen == AppScreen::Assemble) {
                assemble = AssembleState{};
            }

            if (!nav.running) {
                emit(nav, flow, review, assemble, entries, json::array(), "", true);
                return 0;
            }

            emit(nav, flow, review, assemble, entries, json::array());
            continue;
        }

        // ── Validate screen matches current nav ──────────────────────────
        auto current_screen = screen_name(nav.screen);
        if (screen_target != current_screen && nav.screen != AppScreen::Session) {
            emit(nav, flow, review, assemble, entries, json::array(),
                 "wrong screen: currently on " + current_screen);
            continue;
        }
        // Session screen has no screen-level commands
        if (nav.screen == AppScreen::Session) {
            emit(nav, flow, review, assemble, entries, json::array(),
                 "wrong screen: currently on session — use screen:nav for navigation");
            continue;
        }

        // ── RECORDING screen ─────────────────────────────────────────────
        if (screen_target == "recording") {
            std::optional<RecordingCmd> cmd;
            if (input == "CountdownComplete")
                cmd = RecordingCmd::CountdownComplete;
            else
                cmd = parse_recording_cmd(input);

            if (!cmd) {
                emit(nav, flow, review, assemble, entries, json::array(),
                     "unknown command: " + input);
                continue;
            }

            auto t = recording_step(flow, *cmd);
            flow = t.next;

            // Reflect take creation/removal in fixture
            for (const auto& eff : t.effects) {
                if (std::holds_alternative<StopRecording>(eff)) {
                    entries[flow.current_idx].raw_take_path = "fake_take.wav";
                    flow.has_take = true;
                }
                if (std::holds_alternative<ClearTake>(eff)) {
                    entries[flow.current_idx].raw_take_path = "";
                    flow.has_take = false;
                }
            }

            json effs = json::array();
            for (const auto& eff : t.effects)
                effs.push_back(recording_effect_name(eff));

            emit(nav, flow, review, assemble, entries, effs);
            continue;
        }

        // ── REVIEW screen ────────────────────────────────────────────────
        if (screen_target == "review") {
            auto cmd = parse_review_cmd(input);
            if (!cmd) {
                emit(nav, flow, review, assemble, entries, json::array(),
                     "unknown command: " + input);
                continue;
            }

            int idx   = std::clamp(review.selected_idx, 0, (int)entries.size() - 1);
            bool has  = !entries[idx].raw_take_path.empty();
            std::filesystem::path path = has ? entries[idx].raw_take_path : "";

            auto t = review_step(review, *cmd, has, path);
            review = t.next;

            // Store selected index for ReviewGoRecord payload
            nav.review_idx = review.selected_idx;

            json effs = json::array();
            for (const auto& eff : t.effects)
                effs.push_back(review_effect_name(eff));

            emit(nav, flow, review, assemble, entries, effs);
            continue;
        }

        // ── ASSEMBLE screen ──────────────────────────────────────────────
        if (screen_target == "assemble") {
            auto cmd = parse_assemble_cmd_str(input);
            if (!cmd) {
                emit(nav, flow, review, assemble, entries, json::array(),
                     "unknown command: " + input);
                continue;
            }

            auto t = assemble_step(assemble, *cmd);
            assemble = t.next;

            json effs = json::array();
            for (const auto& eff : t.effects)
                effs.push_back(assemble_effect_name(eff));

            emit(nav, flow, review, assemble, entries, effs);
            continue;
        }

        // ── Unknown screen target ────────────────────────────────────────
        emit(nav, flow, review, assemble, entries, json::array(),
             "unknown screen: " + screen_target);
    }

    return 0;
}
