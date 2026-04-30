#include "core/recording_flow.hpp"
#include "core/recording_effects.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

using json = nlohmann::json;
using namespace core;

// Convert FlowPhase to string
static std::string phase_to_string(FlowPhase p) {
    switch (p) {
        case FlowPhase::Idle: return "idle";
        case FlowPhase::Countdown: return "countdown";
        case FlowPhase::Recording: return "recording";
    }
    return "unknown";
}

// Fake entry data for standalone harness
struct FakeEntry {
    int index;
    std::string text;
    int64_t start_ms;
    int64_t end_ms;
    int64_t slot_duration_ms;
    std::string status;
};

// Minimal fixture: 3 fake entries
static std::vector<FakeEntry> create_fixture() {
    return {
        {1, "Hello world", 1000, 3000, 2000, "pending"},
        {2, "How are you?", 3500, 5500, 2000, "pending"},
        {3, "Goodbye!", 6000, 8000, 2000, "pending"},
    };
}

// Emit current state as JSON
static void emit_state(const FlowState& state, const std::vector<FakeEntry>& entries) {
    const auto& entry = entries[state.current_idx];
    json j = {
        {"current_idx", state.current_idx},
        {"total", state.total},
        {"phase", phase_to_string(state.phase)},
        {"has_take", state.has_take},
        {"entry", {
            {"index", entry.index},
            {"text", entry.text},
            {"start_ms", entry.start_ms},
            {"end_ms", entry.end_ms},
            {"slot_duration_ms", entry.slot_duration_ms},
            {"status", entry.status}
        }}
    };
    std::cout << j.dump(2) << std::endl;
}

int main(int argc, char** argv) {
    // For now, always use built-in fixture
    // Future enhancement: load from args if provided
    (void)argc;
    (void)argv;

    auto entries = create_fixture();
    FlowState state{
        .current_idx = 0,
        .total = static_cast<int>(entries.size()),
        .phase = FlowPhase::Idle,
        .has_take = false
    };

    // Emit initial state
    emit_state(state, entries);

    // Main loop: read JSON commands from stdin
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        try {
            auto j = json::parse(line);
            if (!j.contains("input")) {
                json err = {{"error", "missing 'input' field"}};
                std::cout << err.dump() << std::endl;
                continue;
            }

            std::string input = j["input"];
            std::optional<RecordingCmd> cmd;

            // Special handling for synthetic CountdownComplete
            if (input == "CountdownComplete") {
                cmd = RecordingCmd::CountdownComplete;
            } else {
                cmd = parse_recording_cmd(input);
            }

            if (!cmd.has_value()) {
                json err = {{"error", "unknown command"}};
                std::cout << err.dump() << std::endl;
                continue;
            }

            // Apply the transition
            auto transition = recording_step(state, cmd.value());
            state = transition.next;

            // Update has_take based on index change (in real app, this comes from project)
            // For harness, we just reset it to false on navigation
            if (cmd == RecordingCmd::Next || cmd == RecordingCmd::Back) {
                state.has_take = false;
            }
            // Simulate take creation on StopRecording
            bool has_stop_recording = false;
            bool has_clear_take = false;
            bool has_exit = false;
            for (const auto& eff : transition.effects) {
                if (std::holds_alternative<core::StopRecording>(eff)) has_stop_recording = true;
                if (std::holds_alternative<core::ClearTake>(eff))     has_clear_take = true;
                if (std::holds_alternative<core::ExitToSession>(eff)) has_exit = true;
            }
            if (has_stop_recording) state.has_take = true;
            if (has_clear_take)     state.has_take = false;

            // Build effects array for JSON output
            std::string effects_json = "[";
            bool first_eff = true;
            for (const auto& eff : transition.effects) {
                if (!first_eff) effects_json += ",";
                first_eff = false;
                std::visit([&](const auto& e) {
                    using T = std::decay_t<decltype(e)>;
                    if constexpr (std::is_same_v<T, core::StartCountdown>)    effects_json += "\"StartCountdown\"";
                    else if constexpr (std::is_same_v<T, core::CancelCountdown>) effects_json += "\"CancelCountdown\"";
                    else if constexpr (std::is_same_v<T, core::ActivateCapture>) effects_json += "\"ActivateCapture\"";
                    else if constexpr (std::is_same_v<T, core::StopRecording>)   effects_json += "\"StopRecording\"";
                    else if constexpr (std::is_same_v<T, core::PlayTake>)        effects_json += "\"PlayTake\"";
                    else if constexpr (std::is_same_v<T, core::StopPlayback>)    effects_json += "\"StopPlayback\"";
                    else if constexpr (std::is_same_v<T, core::SaveTake>)        effects_json += "\"SaveTake\"";
                    else if constexpr (std::is_same_v<T, core::ClearTake>)       effects_json += "\"ClearTake\"";
                    else if constexpr (std::is_same_v<T, core::SaveProject>)     effects_json += "\"SaveProject\"";
                    else if constexpr (std::is_same_v<T, core::ExitToSession>)   effects_json += "\"ExitToSession\"";
                }, eff);
            }
            effects_json += "]";

            // Check for exit
            if (has_exit) {
                std::cout << "{\"current_idx\":" << state.current_idx
                          << ",\"total\":" << state.total
                          << ",\"phase\":\"" << phase_to_string(state.phase) << "\""
                          << ",\"has_take\":" << (state.has_take ? "true" : "false")
                          << ",\"effects\":" << effects_json
                          << ",\"exit\":true}" << std::endl;
                return 0;
            }

            // Emit new state with effects
            const auto& entry = entries[state.current_idx];
            json out = {
                {"current_idx", state.current_idx},
                {"total", state.total},
                {"phase", phase_to_string(state.phase)},
                {"has_take", state.has_take},
                {"entry", {
                    {"index", entry.index},
                    {"text", entry.text},
                    {"start_ms", entry.start_ms},
                    {"end_ms", entry.end_ms},
                    {"slot_duration_ms", entry.slot_duration_ms},
                    {"status", entry.status}
                }}
            };
            std::string out_str = out.dump(2);
            // Insert effects before closing brace
            out_str.insert(out_str.rfind('}'), ",\n  \"effects\": " + effects_json + "\n");
            std::cout << out_str << std::endl;

        } catch (const json::exception& e) {
            json err = {{"error", std::string("JSON parse error: ") + e.what()}};
            std::cout << err.dump() << std::endl;
        }
    }

    return 0;
}
