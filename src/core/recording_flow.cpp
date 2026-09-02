#include "core/recording_flow.hpp"
#include "core/recording_render_state.hpp"

namespace core {

std::optional<RecordingCmd> parse_recording_cmd(const std::string& key) {
    if (key == "r") return RecordingCmd::Record;
    if (key == "s") return RecordingCmd::Stop;
    if (key == "p") return RecordingCmd::Play;
    if (key == "x") return RecordingCmd::Redo;
    if (key == "n") return RecordingCmd::Next;
    if (key == "b") return RecordingCmd::Back;
    if (key == "q") return RecordingCmd::Quit;
    return std::nullopt;
}

FlowTransition recording_step(FlowState state, RecordingCmd cmd) {
    FlowTransition result{.next = state, .effects = {}};
    auto& effects = result.effects;

    switch (cmd) {
        case RecordingCmd::Record:
            if (state.phase == FlowPhase::Idle) {
                result.next.phase = FlowPhase::Countdown;
                effects.push_back(StartCountdown{state.current_idx});
            }
            break;

        case RecordingCmd::Stop:
            if (state.phase == FlowPhase::Countdown) {
                result.next.phase = FlowPhase::Idle;
                effects.push_back(CancelCountdown{});
            } else if (state.phase == FlowPhase::Recording) {
                result.next.phase = FlowPhase::Idle;
                effects.push_back(StopRecording{state.current_idx, {}});
            }
            break;

        case RecordingCmd::Play:
            if (state.phase == FlowPhase::Idle && state.has_take) {
                effects.push_back(PlayTake{{}});
            }
            break;

        case RecordingCmd::Redo:
            if (state.phase == FlowPhase::Countdown) {
                effects.push_back(CancelCountdown{});
            } else if (state.phase == FlowPhase::Recording) {
                effects.push_back(StopRecording{state.current_idx, {}});
            }
            result.next.phase = FlowPhase::Idle;
            effects.push_back(StopPlayback{});
            effects.push_back(ClearTake{state.current_idx});
            effects.push_back(SaveProject{});
            result.next.has_take = false;
            break;

        case RecordingCmd::Next:
            if (state.phase == FlowPhase::Countdown) {
                effects.push_back(CancelCountdown{});
            } else if (state.phase == FlowPhase::Recording) {
                effects.push_back(StopRecording{state.current_idx, {}});
            }
            result.next.phase = FlowPhase::Idle;
            effects.push_back(StopPlayback{});
            if (state.current_idx + 1 < state.total) {
                result.next.current_idx = state.current_idx + 1;
            }
            break;

        case RecordingCmd::Back:
            if (state.phase == FlowPhase::Countdown) {
                effects.push_back(CancelCountdown{});
            } else if (state.phase == FlowPhase::Recording) {
                effects.push_back(StopRecording{state.current_idx, {}});
            }
            result.next.phase = FlowPhase::Idle;
            effects.push_back(StopPlayback{});
            if (state.current_idx > 0) {
                result.next.current_idx = state.current_idx - 1;
            }
            break;

        case RecordingCmd::Quit:
            if (state.phase == FlowPhase::Countdown) {
                effects.push_back(CancelCountdown{});
            } else if (state.phase == FlowPhase::Recording) {
                effects.push_back(StopRecording{state.current_idx, {}});
            }
            result.next.phase = FlowPhase::Idle;
            effects.push_back(StopPlayback{});
            effects.push_back(ExitToSession{});
            break;

        case RecordingCmd::CountdownComplete:
            if (state.phase == FlowPhase::Countdown) {
                result.next.phase = FlowPhase::Recording;
                effects.push_back(ActivateCapture{});
            }
            break;

        case RecordingCmd::CountdownFailed:
            if (state.phase == FlowPhase::Countdown) {
                result.next.phase = FlowPhase::Idle;
                effects.push_back(CancelCountdown{});
            }
            break;
    }

    return result;
}

RecordingRenderState render_state(const FlowState& state,
                                  const std::string& text,
                                  int64_t slot_ms,
                                  int64_t elapsed) {
    std::string label;
    switch (state.phase) {
        case FlowPhase::Idle:       label = "idle";       break;
        case FlowPhase::Countdown:  label = "countdown";  break;
        case FlowPhase::Recording:  label = "recording";  break;
    }
    return RecordingRenderState{
        .current_idx      = state.current_idx,
        .total            = state.total,
        .phase_label      = std::move(label),
        .has_take         = state.has_take,
        .entry_text       = text,
        .slot_duration_ms = slot_ms,
        .elapsed_ms       = elapsed,
    };
}

} // namespace core
