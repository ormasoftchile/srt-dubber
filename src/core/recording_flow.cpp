#include "core/recording_flow.hpp"

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

    switch (cmd) {
        case RecordingCmd::Record:
            if (state.phase == FlowPhase::Idle) {
                result.next.phase = FlowPhase::Countdown;
                result.effects.start_countdown = true;
            }
            break;

        case RecordingCmd::Stop:
            if (state.phase == FlowPhase::Countdown) {
                result.next.phase = FlowPhase::Idle;
                result.effects.cancel_countdown = true;
            } else if (state.phase == FlowPhase::Recording) {
                result.next.phase = FlowPhase::Idle;
                result.effects.stop_recording = true;
            }
            break;

        case RecordingCmd::Play:
            if (state.phase == FlowPhase::Idle && state.has_take) {
                result.effects.play_take = true;
            }
            break;

        case RecordingCmd::Redo:
            if (state.phase == FlowPhase::Countdown) {
                result.effects.cancel_countdown = true;
            } else if (state.phase == FlowPhase::Recording) {
                result.effects.stop_recording = true;
            }
            result.next.phase = FlowPhase::Idle;
            result.effects.stop_playback = true;
            result.effects.clear_take = true;
            result.next.has_take = false;
            break;

        case RecordingCmd::Next:
            if (state.phase == FlowPhase::Countdown) {
                result.effects.cancel_countdown = true;
            } else if (state.phase == FlowPhase::Recording) {
                result.effects.stop_recording = true;
            }
            result.next.phase = FlowPhase::Idle;
            result.effects.stop_playback = true;
            if (state.current_idx + 1 < state.total) {
                result.next.current_idx = state.current_idx + 1;
            }
            break;

        case RecordingCmd::Back:
            if (state.phase == FlowPhase::Countdown) {
                result.effects.cancel_countdown = true;
            } else if (state.phase == FlowPhase::Recording) {
                result.effects.stop_recording = true;
            }
            result.next.phase = FlowPhase::Idle;
            result.effects.stop_playback = true;
            if (state.current_idx > 0) {
                result.next.current_idx = state.current_idx - 1;
            }
            break;

        case RecordingCmd::Quit:
            if (state.phase == FlowPhase::Countdown) {
                result.effects.cancel_countdown = true;
            } else if (state.phase == FlowPhase::Recording) {
                result.effects.stop_recording = true;
            }
            result.next.phase = FlowPhase::Idle;
            result.effects.stop_playback = true;
            result.effects.exit_to_session = true;
            break;

        case RecordingCmd::CountdownComplete:
            if (state.phase == FlowPhase::Countdown) {
                result.next.phase = FlowPhase::Recording;
                result.effects.activate_capture = true;
            }
            break;
    }

    return result;
}

} // namespace core
