#include "core/assemble_flow.hpp"

namespace core {

std::optional<AssembleCmd> parse_assemble_cmd(const std::string& key) {
    if (key == "q" || key == "b") return AssembleCmd::Back;
    return std::nullopt;
}

AssembleTransition assemble_step(AssembleState       state,
                                  AssembleCmd         cmd,
                                  const std::string&  payload) {
    switch (cmd) {
        case AssembleCmd::Start: {
            if (state.phase != AssemblePhase::Idle)
                return {state, {}};
            state.phase = AssemblePhase::Assembling;
            return {state, {SpawnAssembly{}}};
        }

        case AssembleCmd::ProgressLine: {
            if (state.phase != AssemblePhase::Assembling)
                return {state, {}};
            state.log_lines.push_back(payload);
            return {state, {AppendLog{payload}}};
        }

        case AssembleCmd::AssemblyDone: {
            if (state.phase != AssemblePhase::Assembling)
                return {state, {}};
            state.phase       = AssemblePhase::Complete;
            state.output_path = payload;
            return {state, {SaveProject{}}};
        }

        case AssembleCmd::AssemblyFailed: {
            if (state.phase != AssemblePhase::Assembling)
                return {state, {}};
            state.phase     = AssemblePhase::Failed;
            state.error_msg = payload;
            return {state, {}};
        }

        case AssembleCmd::Back: {
            return {state, {ExitToSession{}}};
        }
    }
    return {state, {}};
}

AssembleRenderState assemble_render_state(const AssembleState& state) {
    std::string phase_label;
    std::string footer;

    switch (state.phase) {
        case AssemblePhase::Idle:
            phase_label = "idle";
            footer      = "Idle — press any key to begin";
            break;
        case AssemblePhase::Assembling:
            phase_label = "assembling";
            footer      = "Assembling...";
            break;
        case AssemblePhase::Complete:
            phase_label = "complete";
            footer      = "✓ Complete: " + state.output_path;
            break;
        case AssemblePhase::Failed:
            phase_label = "failed";
            footer      = "✗ Failed: " + state.error_msg;
            break;
    }

    return AssembleRenderState{
        .phase_label = phase_label,
        .log_lines   = state.log_lines,
        .footer      = footer,
        .allow_back  = true,
    };
}

} // namespace core
